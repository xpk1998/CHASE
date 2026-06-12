use ethers_core::types::U256;
use narwhal_types::BatchDigest;
use sslab_execution::types::{EthereumTransaction, ExecutableEthereumBatch};

use super::controller::PipelineController;
use super::stages::{PipelineBatch, StageId};
use crate::executor::config::PipelineConfig;

fn make_batch(gas_per_tx: u64, tx_count: usize) -> PipelineBatch {
    let txs: Vec<EthereumTransaction> = (0..tx_count)
        .map(|_| {
            let mut tx = EthereumTransaction::default();
            tx.0.set_gas(U256::from(gas_per_tx));
            tx
        })
        .collect();
    let batch = ExecutableEthereumBatch::new(txs, BatchDigest::default());
    PipelineBatch::new(1, batch)
}

#[test]
fn pipeline_controller_cold_start_window_is_one() {
    let ctrl = PipelineController::new(&PipelineConfig::default());
    assert_eq!(ctrl.get_window_size(StageId::Order), 1);
    assert_eq!(ctrl.get_window_size(StageId::Exec), 1);
    assert_eq!(ctrl.get_window_size(StageId::Commit), 1);
}

#[test]
fn pipeline_controller_aimd_exponential_increase_on_idle() {
    let config = PipelineConfig {
        zeta_max: 8,
        lambda2: 1_000_000,
        lambda3: 1_000_000,
    };
    let ctrl = PipelineController::new(&config);

    ctrl.on_batch_exit(StageId::Order, 100);

    assert_eq!(ctrl.get_window_size(StageId::Order), 2);
}

#[test]
fn pipeline_controller_aimd_multiplicative_decrease_on_overload() {
    let config = PipelineConfig {
        zeta_max: 8,
        lambda2: 100,
        lambda3: 1_000_000,
    };
    let ctrl = PipelineController::new(&config);

    ctrl.on_batch_enter(StageId::Exec, 200);
    ctrl.on_batch_exit(StageId::Order, 50);

    assert_eq!(ctrl.get_window_size(StageId::Order), 0);
}

#[test]
fn pipeline_batch_gas_weight() {
    let batch = make_batch(21_000, 3);
    assert_eq!(batch.gas_weight, 21_000 * 3);
}

#[test]
fn pipeline_controller_linear_increase_under_light_load() {
    let config = PipelineConfig {
        zeta_max: 8,
        lambda2: 10_000,
        lambda3: 10_000,
    };
    let ctrl = PipelineController::new(&config);

    ctrl.on_batch_enter(StageId::Exec, 500);
    ctrl.on_batch_exit(StageId::Order, 100);

    assert_eq!(ctrl.get_window_size(StageId::Order), 2);
}
