//! Seer-accelerated transaction simulation for Chase pre-execution.

use ethers_core::types::Address;
use evm::backend::{Apply, Log};
use evm::executor::stack::RwSet;
use sslab_execution::{
    evm_storage::{backend::ApplyBackend, EvmStorage},
    types::{EthereumTransaction, IndexedEthereumTransaction},
};
use evm::backend::Backend;

use crate::types::{extract_read_set, extract_write_set, SimulatedTransaction};

use super::context::SeerContext;
use super::pre_execution_cache::CachedSimulation;

/// Reorder transactions by target contract to improve branch-predictor warmup
/// (analogous to Seer's sequential pre-execution over a shared VarTable).
pub fn order_for_contract_locality(
    mut txs: Vec<IndexedEthereumTransaction>,
) -> Vec<IndexedEthereumTransaction> {
    txs.sort_unstable_by(|a, b| {
        let a_to = a.data().to_addr().copied().unwrap_or(Address::zero());
        let b_to = b.data().to_addr().copied().unwrap_or(Address::zero());
        a_to.cmp(&b_to).then_with(|| a.id.cmp(&b.id))
    });
    txs
}

/// Simulate a single transaction with Seer fast-path (cache) and post-sim learning.
pub fn seer_simulate_tx<B>(
    tx: &EthereumTransaction,
    snapshot: &EvmStorage<B>,
    seer: &SeerContext,
) -> Result<Option<(Vec<Apply>, Vec<Log>, RwSet)>, sui_types::error::SuiError>
where
    B: Backend + ApplyBackend + Default + Clone,
{
    if seer.config.enable_cache {
        if let Some(cached) = seer.pre_execution_cache.get(tx) {
            return Ok(Some((cached.effects, cached.logs, cached.rw_set)));
        }
    }

    let result = crate::evm_utils::simulate_tx(tx, snapshot)?;

    if let Some((ref effects, ref logs, ref rw_set)) = result {
        if seer.config.enable_cache {
            seer.pre_execution_cache.insert(
                tx,
                CachedSimulation {
                    effects: effects.clone(),
                    logs: logs.clone(),
                    rw_set: rw_set.clone(),
                },
            );
        }

        if seer.config.enable_perceptron {
            if let Some(to) = tx.to_addr() {
                let read_keys = extract_read_set(rw_set);
                let write_keys = extract_write_set(rw_set);
                seer.var_table
                    .learn_from_rw_access(*to, &read_keys, &write_keys);
            }
        }
    }

    Ok(result)
}

/// Batch simulation with optional Seer acceleration.
pub fn seer_simulate_batch<B>(
    tx_list: Vec<IndexedEthereumTransaction>,
    snapshot: &EvmStorage<B>,
    seer: &SeerContext,
) -> Vec<SimulatedTransaction>
where
    B: Backend + ApplyBackend + Default + Clone + Send + Sync,
{
    let ordered = if seer.config.contract_locality_ordering {
        order_for_contract_locality(tx_list)
    } else {
        tx_list
    };

    ordered
        .into_iter()
        .filter_map(|tx| {
            match seer_simulate_tx(tx.data(), snapshot, seer) {
                Ok(Some((effect, log, rw_set))) => {
                    Some(SimulatedTransaction::new(rw_set, effect, log, tx))
                }
                _ => None,
            }
        })
        .collect()
}
