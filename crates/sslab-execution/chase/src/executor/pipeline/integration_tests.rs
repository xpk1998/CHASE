use std::sync::Arc;

use evm::backend::Apply;
use ethers_core::types::U256;
use narwhal_types::BatchDigest;
use sslab_execution::types::{EthereumTransaction, ExecutableEthereumBatch};

use super::controller::PipelineController;
use super::ev_blp::{BatchExecutor, EvBlpPipeline};
use super::stages::{PipelineBatch, StageId};
use crate::executor::cache::{applies_to_temp_buffer, InMemoryStateStore, TempBuffer};
use crate::executor::config::{EvBlpConfig, PipelineConfig};

struct MockExecutor {
    exec_count: std::sync::atomic::AtomicUsize,
}

impl BatchExecutor for MockExecutor {
    fn execute_batch(
        &self,
        batch: &ExecutableEthereumBatch,
        temp_buffer: &mut TempBuffer,
        _visible_up_to: u64,
    ) -> BatchDigest {
        self.exec_count
            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        for (i, _tx) in batch.data().iter().enumerate() {
            let addr = ethers_core::types::H160::from_low_u64_be(i as u64 + 1);
            let slot = ethers_core::types::H256::from_low_u64_be(i as u64 + 1);
            let val = ethers_core::types::H256::from_low_u64_be(i as u64 + 2);
            temp_buffer.record_write(addr, slot, val);
        }
        *batch.digest()
    }

    fn commit_batch(&self, _batch: &PipelineBatch) -> Result<(), String> {
        Ok(())
    }
}

fn make_batches(count: usize) -> Vec<ExecutableEthereumBatch> {
    (0..count)
        .map(|i| {
            let mut tx = EthereumTransaction::default();
            tx.0.set_gas(U256::from(21_000u64));
            ExecutableEthereumBatch::new(
                vec![tx],
                BatchDigest::new([i as u8; 32]),
            )
        })
        .collect()
}

#[test]
fn pipeline_processes_multiple_batches() {
    let store = Arc::new(InMemoryStateStore::default());
    let pipeline = EvBlpPipeline::from_config(EvBlpConfig::default(), store);
    let executor = MockExecutor {
        exec_count: std::sync::atomic::AtomicUsize::new(0),
    };

    let batches = make_batches(3);
    let digests = pipeline.process_batches(batches, &executor);
    assert_eq!(digests.len(), 3);
    assert_eq!(
        executor.exec_count.load(std::sync::atomic::Ordering::Relaxed),
        3
    );
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
