use evm::backend::Apply;
use ethers_providers::{MockProvider, Provider};
use sslab_execution::{
    utils::{
        smallbank_contract_benchmark::concurrent_evm_storage,
        test_utils::{SmallBankTransactionHandler, DEFAULT_CHAIN_ID},
    },
};

use super::controller::PipelineController;
use super::stages::StageId;
use crate::chase_core::ConcurrencyLevelManager;
use crate::executor::cache::{applies_to_temp_buffer, TempBuffer};
use crate::executor::chase_bridge::EvBlpChaseBridge;
use crate::executor::config::PipelineConfig;

fn smallbank_batches(count: usize) -> Vec<sslab_execution::types::ExecutableEthereumBatch> {
    let provider = Provider::<MockProvider>::new(MockProvider::default());
    let handler = SmallBankTransactionHandler::new(provider, DEFAULT_CHAIN_ID);
    handler.create_batches(1, count, 0.0, 100_000)
}

#[tokio::test]
async fn pipeline_processes_multiple_batches() {
    let manager = ConcurrencyLevelManager::new(concurrent_evm_storage(), 2);
    let bridge = EvBlpChaseBridge::new(manager, None);
    let digests = bridge.execute(smallbank_batches(3)).await;
    assert_eq!(digests.len(), 3);
}

#[test]
fn pipeline_controller_tracks_in_flight_batches() {
    let ctrl = PipelineController::new(&PipelineConfig {
        zeta_max: 2,
        lambda2: 10_000_000,
        lambda3: 10_000_000,
    });

    ctrl.on_batch_enter(StageId::Exec, 1000);
    assert_eq!(ctrl.in_flight_count(StageId::Exec), 1);
    ctrl.on_batch_exit(StageId::Exec, 1000);
    assert_eq!(ctrl.in_flight_count(StageId::Exec), 0);
}

#[test]
fn applies_to_temp_buffer_extracts_storage_writes() {
    use ethers_core::types::{H160, H256};
    use std::collections::BTreeMap;

    let mut storage = BTreeMap::new();
    storage.insert(H256::from_low_u64_be(1), H256::from_low_u64_be(42));

    let applies = vec![Apply::Modify {
        address: H160::from_low_u64_be(7),
        basic: evm::backend::Basic::default(),
        code: None,
        storage,
        reset_storage: false,
    }];

    let mut buf = TempBuffer::new();
    applies_to_temp_buffer(&applies, &mut buf);
    assert_eq!(buf.delta_bytes(), 64);
}

#[test]
fn pipeline_respects_zeta_max_concurrency() {
    let ctrl = PipelineController::new(&PipelineConfig {
        zeta_max: 2,
        lambda2: 10_000_000,
        lambda3: 10_000_000,
    });

    ctrl.on_batch_enter(StageId::Exec, 100);
    ctrl.on_batch_enter(StageId::Exec, 100);
    assert!(!ctrl.can_push(StageId::Exec));
    assert_eq!(ctrl.in_flight_count(StageId::Exec), 2);

    ctrl.on_batch_exit(StageId::Exec, 100);
    assert!(ctrl.can_push(StageId::Exec));
}

#[test]
fn pipeline_backpressure_closes_window_on_overload() {
    let ctrl = PipelineController::new(&PipelineConfig {
        zeta_max: 8,
        lambda2: 1_000,
        lambda3: 10_000_000,
    });

    ctrl.on_batch_enter(StageId::Exec, 1_500);
    ctrl.on_batch_exit(StageId::Order, 100);
    assert_eq!(ctrl.get_window_size(StageId::Order), 0);

    ctrl.on_batch_exit(StageId::Exec, 1_500);
    ctrl.on_batch_exit(StageId::Order, 50);
    assert!(ctrl.get_window_size(StageId::Order) >= 1);
}

#[test]
fn lambda_recommendation_from_samples() {
    use super::calibration::recommend_lambdas;
    use super::metrics::WorkloadSample;

    let samples: Vec<WorkloadSample> = (0..20)
        .map(|i| WorkloadSample {
            gas_weight: 50_000 * (i + 1),
            delta_bytes: 1024 * (i + 1),
        })
        .collect();
    let rec = recommend_lambdas(&samples, 4);
    assert!(rec.lambda2 > 0);
    assert!(rec.lambda3 > 0);
    assert_eq!(rec.sample_count, 20);
}
