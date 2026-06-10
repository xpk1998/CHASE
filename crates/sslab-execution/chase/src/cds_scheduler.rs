//! CDS (Conflict Detection Scheduling) scheduler for CHASE.
//!
//! Maps CHASE commit waves onto two CDS zones:
//! - **Conflict-free zone** (commit wave 1): parallel dependency chains extracted
//!   by stringing together dependent transactions within the wave.
//! - **Conflict zone** (commit waves 2+): dependency chains placed via inter-epoch
//!   reordering (minimum non-conflicting epoch search).

use std::sync::Arc;

use ethers_core::types::H256;
use hashbrown::HashSet;
use rayon::prelude::*;

use crate::address_based_conflict_graph::{FastHashMap, Transaction};
use crate::types::{AbortedTransaction, FinalizedTransaction};

/// Transaction with RW keys used for CDS chain / epoch placement.
#[derive(Clone)]
struct TxWithKeys {
    id: u64,
    read_keys: HashSet<H256>,
    write_keys: HashSet<H256>,
    finalized: FinalizedTransaction,
}

impl TxWithKeys {
    fn from_transaction(tx: &Arc<Transaction>) -> Self {
        let read_keys = tx.abort_info.read().read_keys();
        let write_keys = tx.abort_info.read().write_keys();
        let finalized = tx.to_finalized();
        Self {
            id: finalized.id(),
            read_keys,
            write_keys,
            finalized,
        }
    }
}

/// Group non-aborted transactions by CHASE commit wave (sequence).
pub fn group_by_sequence(
    tx_list: FastHashMap<u64, Arc<Transaction>>,
    rayon: bool,
) -> Vec<Vec<Arc<Transaction>>> {
    let mut list = if rayon {
        tx_list.into_par_iter().map(|(_, tx)| tx).collect::<Vec<_>>()
    } else {
        tx_list.into_iter().map(|(_, tx)| tx).collect::<Vec<_>>()
    };

    list.sort_unstable_by_key(|tx| (tx.sequence(), tx.id()));

    if list.is_empty() {
        return Vec::new();
    }

    let mut groups: Vec<Vec<Arc<Transaction>>> = vec![vec![list[0].clone()]];
    for tx in list.iter().skip(1) {
        let current_seq = groups.last().unwrap()[0].sequence();
        if tx.sequence() == current_seq {
            groups.last_mut().unwrap().push(tx.clone());
        } else {
            groups.push(vec![tx.clone()]);
        }
    }
    groups
}

/// CDS conflict-free zone: extract parallel dependency chains within the first commit wave.
///
/// Within each chain, transactions with RW dependencies are serialized; independent
/// chains execute in parallel.
pub fn extract_conflict_free_chains(
    seq1_txs: Vec<Arc<Transaction>>,
    rayon: bool,
) -> Vec<Vec<FinalizedTransaction>> {
    let mut txs: Vec<TxWithKeys> = if rayon {
        seq1_txs
            .par_iter()
            .map(TxWithKeys::from_transaction)
            .collect()
    } else {
        seq1_txs.iter().map(TxWithKeys::from_transaction).collect()
    };

    if txs.is_empty() {
        return vec![];
    }

    txs.sort_unstable_by_key(|tx| tx.id);

    build_conflict_free_chain_indices(&txs)
        .into_iter()
        .map(|chain_indices| {
            chain_indices
                .into_iter()
                .map(|i| txs[i].finalized.clone())
                .collect()
        })
        .collect()
}

/// CDS conflict zone for CHASE commit waves 2+: inter-epoch reordering placement.
pub fn schedule_conflict_zone_finalized(
    seq2plus_txs: Vec<Arc<Transaction>>,
    rayon: bool,
) -> Vec<Vec<FinalizedTransaction>> {
    let mut candidates: Vec<TxWithKeys> = if rayon {
        seq2plus_txs
            .par_iter()
            .map(TxWithKeys::from_transaction)
            .collect()
    } else {
        seq2plus_txs.iter().map(TxWithKeys::from_transaction).collect()
    };

    candidates.sort_unstable_by_key(|tx| tx.id);
    place_into_epochs(&candidates, |tx| tx.finalized.clone())
}

/// CDS conflict zone for aborted transactions: inter-epoch reordering placement.
pub fn schedule_conflict_zone_aborted(
    aborted_txs: Vec<Arc<Transaction>>,
    rayon: bool,
) -> Vec<Vec<AbortedTransaction>> {
    if cfg!(feature = "disable-rescheduling") {
        return vec![];
    }

    if rayon {
        aborted_txs.par_iter().for_each(|tx| {
            tx.clear_write_units();
            tx.init();
        });
    } else {
        aborted_txs.iter().for_each(|tx| {
            tx.clear_write_units();
            tx.init();
        });
    }

    let mut candidates: Vec<(u64, HashSet<H256>, HashSet<H256>, AbortedTransaction)> = if rayon {
        aborted_txs
            .par_iter()
            .map(|tx| {
                let aborted = tx.to_aborted();
                let id = aborted.id();
                let read_keys = aborted.read_keys().clone();
                let write_keys = aborted.write_keys().clone();
                (id, read_keys, write_keys, aborted)
            })
            .collect()
    } else {
        aborted_txs
            .iter()
            .map(|tx| {
                let aborted = tx.to_aborted();
                let id = aborted.id();
                let read_keys = aborted.read_keys().clone();
                let write_keys = aborted.write_keys().clone();
                (id, read_keys, write_keys, aborted)
            })
            .collect()
    };

    candidates.sort_unstable_by_key(|(id, _, _, _)| *id);

    let mut epoch_map: Vec<HashSet<H256>> = vec![];
    let mut schedule: Vec<Vec<AbortedTransaction>> = vec![];

    for (_, read_keys, write_keys, aborted) in candidates {
        let epoch = find_minimum_epoch(&read_keys, &write_keys, &epoch_map);
        match epoch_map.get_mut(epoch) {
            Some(w_map) => {
                w_map.extend(write_keys.iter().cloned());
                schedule[epoch].push(aborted);
            }
            None => {
                epoch_map.push(write_keys);
                schedule.push(vec![aborted]);
            }
        }
    }

    schedule
}

fn place_into_epochs<T: Clone>(
    candidates: &[TxWithKeys],
    map_tx: impl Fn(&TxWithKeys) -> T,
) -> Vec<Vec<T>> {
    let mut epoch_map: Vec<HashSet<H256>> = vec![];
    let mut schedule: Vec<Vec<T>> = vec![];

    for tx in candidates {
        let epoch = find_minimum_epoch(&tx.read_keys, &tx.write_keys, &epoch_map);
        match epoch_map.get_mut(epoch) {
            Some(w_map) => {
                w_map.extend(tx.write_keys.iter().cloned());
                schedule[epoch].push(map_tx(tx));
            }
            None => {
                epoch_map.push(tx.write_keys.clone());
                schedule.push(vec![map_tx(tx)]);
            }
        }
    }

    schedule
}

/// Build dependency chains over `TxWithKeys` (indices into `txs`).
fn build_conflict_free_chain_indices(txs: &[TxWithKeys]) -> Vec<Vec<usize>> {
    let n = txs.len();
    if n == 0 {
        return vec![];
    }

    let mut depends_on: Vec<Vec<usize>> = vec![vec![]; n];
    for i in 0..n {
        for j in 0..n {
            if i != j && !txs[i].read_keys.is_disjoint(&txs[j].write_keys) {
                depends_on[i].push(j);
            }
        }
    }

    let mut assigned = vec![false; n];
    let mut chains = Vec::new();

    while assigned.iter().any(|&a| !a) {
        let start = (0..n)
            .find(|&i| !assigned[i] && depends_on[i].iter().all(|&j| assigned[j]))
            .expect("cycle in conflict-free zone dependency graph");

        let mut chain_indices = vec![start];
        assigned[start] = true;

        loop {
            let last = *chain_indices.last().unwrap();
            let next = (0..n).find(|&i| {
                !assigned[i]
                    && depends_on[i].contains(&last)
                    && depends_on[i].iter().all(|&j| assigned[j])
            });

            match next {
                Some(i) => {
                    chain_indices.push(i);
                    assigned[i] = true;
                }
                None => break,
            }
        }

        chains.push(chain_indices);
    }

    chains
}

/// Inter-epoch reordering: find the minimum epoch with no RW/WW conflicts.
fn find_minimum_epoch(
    read_keys: &HashSet<H256>,
    write_keys: &HashSet<H256>,
    epoch_map: &[HashSet<H256>],
) -> usize {
    let keys_of_tx: HashSet<H256> = read_keys.union(write_keys).cloned().collect();
    let mut epoch = 0;
    while epoch_map.len() > epoch && !keys_of_tx.is_disjoint(&epoch_map[epoch]) {
        epoch += 1;
    }
    epoch
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::FinalizedTransaction;

    fn tx_with_keys(id: u64, reads: &[u64], writes: &[u64]) -> TxWithKeys {
        TxWithKeys {
            id,
            read_keys: reads
                .iter()
                .map(|key| H256::from_low_u64_be(*key))
                .collect(),
            write_keys: writes
                .iter()
                .map(|key| H256::from_low_u64_be(*key))
                .collect(),
            finalized: FinalizedTransaction::new(id, vec![]),
        }
    }

    #[test]
    fn conflict_free_chain_serializes_rw_dependencies() {
        let txs = vec![
            tx_with_keys(1, &[10], &[1]),
            tx_with_keys(2, &[1], &[2]),
            tx_with_keys(3, &[2], &[3]),
        ];

        let chains: Vec<Vec<u64>> = build_conflict_free_chain_indices(&txs)
            .into_iter()
            .map(|chain| chain.into_iter().map(|i| txs[i].id).collect())
            .collect();

        assert_eq!(chains, vec![vec![1, 2, 3]]);
    }

    #[test]
    fn conflict_free_independent_txs_form_parallel_chains() {
        let txs = vec![
            tx_with_keys(1, &[10], &[1]),
            tx_with_keys(2, &[20], &[2]),
        ];

        let chains: Vec<Vec<u64>> = build_conflict_free_chain_indices(&txs)
            .into_iter()
            .map(|chain| chain.into_iter().map(|i| txs[i].id).collect())
            .collect();

        assert_eq!(chains, vec![vec![1], vec![2]]);
    }

    #[test]
    fn inter_epoch_reordering_places_conflicting_txs_in_later_epochs() {
        let txs = vec![
            tx_with_keys(1, &[], &[1]),
            tx_with_keys(2, &[1], &[2]),
        ];

        let schedule = place_into_epochs(&txs, |tx| tx.id);
        assert_eq!(schedule, vec![vec![1], vec![2]]);
    }
}
