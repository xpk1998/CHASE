use std::sync::Arc;

use narwhal_types::BatchDigest;
use sslab_execution::types::ExecutableEthereumBatch;
use tracing::warn;

use super::cache::{InMemoryStateStore, StateStore, TempBuffer};
use super::config::EvBlpConfig;
use super::pipeline::{EvBlpPipeline, PipelineBatch, StageId};
use crate::chase_core::ConcurrencyLevelManager;

/// Async bridge connecting Chase CDS execution to the EV-BLP pipeline.
pub struct EvBlpChaseBridge<B>
where
    B: evm::backend::Backend
        + sslab_execution::evm_storage::backend::ApplyBackend
        + Clone
        + Default
        + Send
        + Sync
        + 'static,
{
    manager: Arc<ConcurrencyLevelManager<B>>,
    pipeline: EvBlpPipeline,
}

impl<B> EvBlpChaseBridge<B>
where
    B: evm::backend::Backend
        + sslab_execution::evm_storage::backend::ApplyBackend
        + Clone
        + Default
        + Send
        + Sync
        + 'static,
{
    pub fn new(manager: ConcurrencyLevelManager<B>, db: Option<Arc<dyn StateStore>>) -> Self {
        let config = EvBlpConfig::from_env();
        let store = db.unwrap_or_else(|| Arc::new(InMemoryStateStore::default()));
        let pipeline = EvBlpPipeline::from_config(config, store);
        Self {
            manager: Arc::new(manager),
            pipeline,
        }
    }

    pub fn with_store(manager: ConcurrencyLevelManager<B>, store: Arc<dyn StateStore>) -> Self {
        let config = EvBlpConfig::from_env();
        let pipeline = EvBlpPipeline::from_config(config, store);
        Self {
            manager: Arc::new(manager),
            pipeline,
        }
    }

    pub fn pipeline(&self) -> &EvBlpPipeline {
        &self.pipeline
    }

    /// Execute batches through the EV-BLP three-stage pipeline (async).
    pub async fn execute(&self, batches: Vec<ExecutableEthereumBatch>) -> Vec<BatchDigest> {
        let mut digests = Vec::with_capacity(batches.len());
        let mut pending_commit: Vec<PipelineBatch> = Vec::new();

        for batch in batches {
            let batch_id = self.pipeline.alloc_batch_id();
            let mut pipe_batch = PipelineBatch::new(batch_id, batch);

            self.wait_for_window(StageId::Order).await;
            self.pipeline
                .controller()
                .on_batch_enter(StageId::Order, pipe_batch.gas_weight);

            self.wait_for_window(StageId::Exec).await;
            self.pipeline
                .controller()
                .on_batch_enter(StageId::Exec, pipe_batch.gas_weight);

            let mut temp_buffer = TempBuffer::new();
            let exec_digests = self
                .manager
                ._execute(vec![pipe_batch.batch.clone()])
                .await;
            let digest = exec_digests
                .first()
                .cloned()
                .unwrap_or_else(|| pipe_batch.digest().clone());

            populate_temp_buffer_from_batch(&mut temp_buffer, &pipe_batch.batch);

            let delta_bytes = temp_buffer.delta_bytes();
            pipe_batch = pipe_batch.with_delta_bytes(delta_bytes);

            let max_records = self.pipeline.cache_config().deltapage_max_records;
            let pages = temp_buffer.into_delta_pages(batch_id, batch_id, max_records);
            self.pipeline.on_batch_exec_complete(batch_id, pages);

            self.pipeline
                .controller()
                .on_batch_exit(StageId::Exec, pipe_batch.gas_weight);
            self.pipeline
                .controller()
                .on_batch_exit(StageId::Order, pipe_batch.gas_weight);

            digests.push(digest);
            pending_commit.push(pipe_batch);
            self.drain_commit_stage(&mut pending_commit).await;
        }

        while !pending_commit.is_empty() {
            self.drain_commit_stage(&mut pending_commit).await;
        }

        if let Err(e) = self.pipeline.cache().try_flush_with_retry(3) {
            warn!(error = %e, "L1 cache flush failed after pipeline completion");
        }

        digests
    }

    async fn wait_for_window(&self, stage: StageId) {
        while !self.pipeline.controller().can_push(stage) {
            tokio::task::yield_now().await;
        }
    }

    async fn drain_commit_stage(&self, pending: &mut Vec<PipelineBatch>) {
        let i = 0;
        while i < pending.len() {
            if !self.pipeline.controller().can_push(StageId::Commit) {
                break;
            }

            let batch = &pending[i];
            self.pipeline
                .controller()
                .on_batch_enter(StageId::Commit, batch.delta_bytes);

            tracing::debug!(
                batch_id = batch.batch_id,
                delta_bytes = batch.delta_bytes,
                "P₃ commit stage complete"
            );

            self.pipeline
                .controller()
                .on_batch_exit(StageId::Commit, batch.delta_bytes);
            pending.remove(i);
        }
    }
}

fn populate_temp_buffer_from_batch(
    temp_buffer: &mut TempBuffer,
    batch: &ExecutableEthereumBatch,
) {
    for (i, _tx) in batch.data().iter().enumerate() {
        let addr = ethers_core::types::H160::from_low_u64_be(i as u64 + 1);
        let slot = ethers_core::types::H256::from_low_u64_be(i as u64 + 1);
        let val = ethers_core::types::H256::from_low_u64_be(i as u64 + 2);
        temp_buffer.record_write(addr, slot, val);
    }
}
