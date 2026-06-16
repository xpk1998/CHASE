use std::sync::Arc;

use narwhal_types::BatchDigest;
use sslab_execution::types::ExecutableEthereumBatch;
use tracing::{debug, warn};

use super::controller::PipelineController;
use super::metrics::PipelineMetrics;
use super::stages::{PipelineBatch, StageId};
use crate::executor::cache::{StateStore, TempBuffer, TwoLevelCache};
use crate::executor::config::{CacheConfig, EvBlpConfig, PipelineConfig};

/// Batch execution handler trait — implemented by Chase integration.
pub trait BatchExecutor: Send + Sync {
    /// Execute a single batch and return its digest. The executor should
    /// populate `temp_buffer` with state writes during execution.
    fn execute_batch(
        &self,
        batch: &ExecutableEthereumBatch,
        temp_buffer: &mut TempBuffer,
        visible_up_to: u64,
    ) -> BatchDigest;

    /// Persist batch results to storage (P₃ commit).
    fn commit_batch(&self, batch: &PipelineBatch) -> Result<(), String>;
}

/// EV-BLP pipeline orchestrator: P₁ Order → P₂ Exec → P₃ Commit.
pub struct EvBlpPipeline {
    controller: Arc<PipelineController>,
    cache: Arc<TwoLevelCache>,
    config: EvBlpConfig,
    metrics: Option<Arc<PipelineMetrics>>,
    next_batch_id: std::sync::atomic::AtomicU64,
    last_exec_complete: std::sync::atomic::AtomicU64,
}

impl EvBlpPipeline {
    pub fn new(
        pipeline_config: PipelineConfig,
        cache_config: CacheConfig,
        db: Arc<dyn StateStore>,
    ) -> Self {
        Self::from_config(EvBlpConfig {
            pipeline: pipeline_config,
            cache: cache_config,
        }, db)
    }

    pub fn from_config(config: EvBlpConfig, db: Arc<dyn StateStore>) -> Self {
        Self::from_config_with_metrics(config, db, None)
    }

    pub fn from_config_with_metrics(
        config: EvBlpConfig,
        db: Arc<dyn StateStore>,
        metrics: Option<Arc<PipelineMetrics>>,
    ) -> Self {
        Self {
            controller: Arc::new(PipelineController::new(&config.pipeline)),
            cache: Arc::new(TwoLevelCache::new(&config.cache, db)),
            config,
            metrics,
            next_batch_id: std::sync::atomic::AtomicU64::new(1),
            last_exec_complete: std::sync::atomic::AtomicU64::new(0),
        }
    }

    pub fn controller(&self) -> &PipelineController {
        &self.controller
    }

    pub fn cache(&self) -> &TwoLevelCache {
        &self.cache
    }

    pub fn cache_arc(&self) -> Arc<TwoLevelCache> {
        self.cache.clone()
    }

    pub fn cache_config(&self) -> &CacheConfig {
        &self.config.cache
    }

    pub fn pipeline_config(&self) -> &PipelineConfig {
        &self.config.pipeline
    }

    pub fn metrics(&self) -> Option<&PipelineMetrics> {
        self.metrics.as_deref()
    }

    pub fn metrics_arc(&self) -> Option<Arc<PipelineMetrics>> {
        self.metrics.clone()
    }

    pub fn alloc_batch_id(&self) -> u64 {
        self.next_batch_id.fetch_add(1, std::sync::atomic::Ordering::Relaxed)
    }

    pub fn completed_batch_id(&self) -> u64 {
        self.last_exec_complete.load(std::sync::atomic::Ordering::SeqCst)
    }

    /// Mark P₂ execution complete — updates L1 visibility and pipeline state.
    pub fn on_batch_exec_complete(&self, batch_id: u64, pages: Vec<super::super::cache::DeltaPage>) {
        self.cache.write_delta_pages(batch_id, pages);
        self.cache.mark_batch_complete(batch_id);
        self.last_exec_complete
            .store(batch_id, std::sync::atomic::Ordering::SeqCst);
    }

    /// Process ordered batches through the three-stage pipeline.
    ///
    /// P₁ produces ordered batches, P₂ executes them (L1 state visible
    /// immediately on completion), P₃ commits asynchronously.
    pub fn process_batches<E: BatchExecutor>(
        &self,
        batches: Vec<ExecutableEthereumBatch>,
        executor: &E,
    ) -> Vec<BatchDigest> {
        let mut digests = Vec::with_capacity(batches.len());
        let mut pending_commit: Vec<PipelineBatch> = Vec::new();

        // P₁: Order — batches arrive pre-ordered from consensus.
        for batch in batches {
            let batch_id = self.alloc_batch_id();
            let mut pipe_batch = PipelineBatch::new(batch_id, batch);

            // Wait for P₁→P₂ window
            while !self.controller.can_push(StageId::Order) {
                self.drain_commit_stage(&mut pending_commit, executor);
            }

            // P₁ enter → P₂
            self.controller
                .on_batch_enter(StageId::Order, pipe_batch.gas_weight);

            // P₂: Execute
            while !self.controller.can_push(StageId::Exec) {
                self.drain_commit_stage(&mut pending_commit, executor);
            }

            let visible_up_to = self.last_exec_complete.load(std::sync::atomic::Ordering::SeqCst);
            self.controller
                .on_batch_enter(StageId::Exec, pipe_batch.gas_weight);

            let mut temp_buffer = TempBuffer::new();
            let digest = executor.execute_batch(
                &pipe_batch.batch,
                &mut temp_buffer,
                visible_up_to,
            );

            let delta_bytes = temp_buffer.delta_bytes();
            pipe_batch = pipe_batch.with_delta_bytes(delta_bytes);

            // Write DeltaPages to L1 and mark execution complete.
            let max_records = self.config.cache.deltapage_max_records;
            let pages = temp_buffer.into_delta_pages(batch_id, batch_id, max_records);
            self.cache.write_delta_pages(batch_id, pages);
            self.cache.mark_batch_complete(batch_id);
            self.last_exec_complete
                .store(batch_id, std::sync::atomic::Ordering::SeqCst);

            // P₂ exit
            self.controller
                .on_batch_exit(StageId::Exec, pipe_batch.gas_weight);
            // P₁ exit
            self.controller
                .on_batch_exit(StageId::Order, pipe_batch.gas_weight);

            digests.push(digest);

            debug!(
                batch_id,
                gas_weight = pipe_batch.gas_weight,
                delta_bytes,
                "P₂ execution complete, L1 state visible"
            );

            pending_commit.push(pipe_batch);
            self.drain_commit_stage(&mut pending_commit, executor);
        }

        // Drain remaining P₃ commits
        while !pending_commit.is_empty() {
            self.drain_commit_stage(&mut pending_commit, executor);
        }

        // Flush any frozen L1 tables
        if let Err(e) = self.cache.try_flush_with_retry(3) {
            warn!(error = %e, "L1 cache flush failed after pipeline completion");
        }

        digests
    }

    /// Process P₃ commits for batches that have window capacity.
    fn drain_commit_stage<E: BatchExecutor>(
        &self,
        pending: &mut Vec<PipelineBatch>,
        executor: &E,
    ) {
        let mut i = 0;
        while i < pending.len() {
            if !self.controller.can_push(StageId::Commit) {
                break;
            }

            let batch = &pending[i];
            self.controller
                .on_batch_enter(StageId::Commit, batch.delta_bytes);

            match executor.commit_batch(batch) {
                Ok(()) => {
                    self.controller
                        .on_batch_exit(StageId::Commit, batch.delta_bytes);
                    pending.remove(i);
                }
                Err(e) => {
                    warn!(batch_id = batch.batch_id, error = %e, "P₃ commit failed");
                    self.controller
                        .on_batch_exit(StageId::Commit, batch.delta_bytes);
                    i += 1;
                }
            }
        }
    }
}
