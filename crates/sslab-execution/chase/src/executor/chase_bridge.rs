use std::sync::Arc;
use std::time::{Duration, Instant};

use narwhal_types::BatchDigest;
use sslab_execution::types::ExecutableEthereumBatch;
use tokio::sync::mpsc;
use tracing::{debug, info, warn};

use super::cache::{applies_to_temp_buffer, StateStore, TempBuffer};
use super::pipeline::{
    recommend_lambdas, EvBlpPipeline, PipelineBatch, PipelineController, PipelineMetrics, StageId,
};
use super::runtime::EvBlpRuntime;
use crate::chase_core::ConcurrencyLevelManager;

/// Shared pipeline state for concurrent stage workers.
pub struct SharedPipeline {
    pub pipeline: Arc<EvBlpPipeline>,
    pub visibility: Arc<super::cache::L1Visibility>,
    pub metrics: Arc<PipelineMetrics>,
}

struct CommitWork {
    batch_id: u64,
    delta_bytes: u64,
}

/// Async bridge with true three-stage concurrent pipelining.
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
    shared: Arc<SharedPipeline>,
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
    pub fn with_runtime(manager: ConcurrencyLevelManager<B>, runtime: EvBlpRuntime) -> Self {
        Self {
            manager: Arc::new(manager),
            shared: Arc::new(SharedPipeline {
                pipeline: runtime.pipeline,
                visibility: runtime.visibility,
                metrics: runtime.metrics,
            }),
        }
    }

    pub fn new(manager: ConcurrencyLevelManager<B>, db: Option<Arc<dyn StateStore>>) -> Self {
        let store = db.unwrap_or_else(|| Arc::new(super::cache::InMemoryStateStore::default()));
        Self::with_runtime(manager, EvBlpRuntime::new(store))
    }

    pub fn pipeline(&self) -> &EvBlpPipeline {
        &self.shared.pipeline
    }

    pub fn metrics(&self) -> &PipelineMetrics {
        &self.shared.metrics
    }

    pub async fn execute(&self, batches: Vec<ExecutableEthereumBatch>) -> Vec<BatchDigest> {
        let n = batches.len();
        if n == 0 {
            return vec![];
        }

        let run_start = Instant::now();
        let (exec_tx, exec_rx) = mpsc::channel::<PipelineBatch>(n);
        let (commit_tx, commit_rx) = mpsc::channel::<CommitWork>(n);
        let (result_tx, result_rx) = mpsc::channel::<(u64, BatchDigest)>(n);

        let shared_exec = self.shared.clone();
        let manager_exec = self.manager.clone();
        let exec_handle = tokio::spawn(async move {
            exec_worker(exec_rx, commit_tx, result_tx, shared_exec, manager_exec).await;
        });

        let shared_commit = self.shared.clone();
        let commit_handle = tokio::spawn(async move {
            commit_worker(commit_rx, shared_commit).await;
        });

        let result_handle = tokio::spawn(async move {
            let mut results = Vec::new();
            let mut rx = result_rx;
            while let Some(item) = rx.recv().await {
                results.push(item);
            }
            results
        });

        for batch in batches {
            let order_start = Instant::now();
            wait_can_push(
                self.shared.pipeline.controller(),
                StageId::Order,
                Some(&self.shared.metrics),
            )
            .await;
            let batch_id = self.shared.pipeline.alloc_batch_id();
            let pipe_batch = PipelineBatch::new(batch_id, batch);
            self.shared
                .pipeline
                .controller()
                .on_batch_enter(StageId::Order, pipe_batch.gas_weight);
            self.shared
                .metrics
                .record_batch(StageId::Order, order_start.elapsed().as_micros() as u64);
            record_backpressure_peak(&self.shared);

            if exec_tx.send(pipe_batch).await.is_err() {
                break;
            }
        }
        drop(exec_tx);

        let _ = exec_handle.await;
        let _ = commit_handle.await;

        let mut results = result_handle.await.unwrap_or_default();
        results.sort_by_key(|(id, _)| *id);

        if let Err(e) = self.shared.pipeline.cache().try_flush_with_retry(3) {
            warn!(error = %e, "L1 cache flush failed after pipeline completion");
        }

        let summary = self.shared.metrics.summary();
        let samples = self.shared.metrics.workload_samples();
        let zeta_max = self.shared.pipeline.pipeline_config().zeta_max;
        let lambda_rec = recommend_lambdas(&samples, zeta_max);

        info!(
            batches = n,
            elapsed_ms = run_start.elapsed().as_millis() as u64,
            report = %summary.format_report(),
            lambda = %lambda_rec.format_report(),
            "EV-BLP pipeline run complete"
        );

        results.into_iter().map(|(_, d)| d).collect()
    }
}

fn record_backpressure_peak(shared: &SharedPipeline) {
    let ctrl = shared.pipeline.controller();
    shared.metrics.record_backpressure_peak(
        ctrl.get_backpressure_weight(StageId::Exec),
        ctrl.get_backpressure_weight(StageId::Commit),
    );
}

async fn exec_worker<B>(
    mut exec_rx: mpsc::Receiver<PipelineBatch>,
    commit_tx: mpsc::Sender<CommitWork>,
    result_tx: mpsc::Sender<(u64, BatchDigest)>,
    shared: Arc<SharedPipeline>,
    manager: Arc<ConcurrencyLevelManager<B>>,
) where
    B: evm::backend::Backend
        + sslab_execution::evm_storage::backend::ApplyBackend
        + Clone
        + Default
        + Send
        + Sync
        + 'static,
{
    while let Some(pipe_batch) = exec_rx.recv().await {
        let batch_id = pipe_batch.batch_id;
        let gas_weight = pipe_batch.gas_weight;

        shared
            .pipeline
            .controller()
            .on_batch_exit(StageId::Order, gas_weight);

        let exec_wait_start = Instant::now();
        wait_can_push(
            shared.pipeline.controller(),
            StageId::Exec,
            Some(&shared.metrics),
        )
        .await;
        shared
            .pipeline
            .controller()
            .on_batch_enter(StageId::Exec, gas_weight);
        record_backpressure_peak(&shared);

        let visible_up_to = shared.pipeline.completed_batch_id();
        shared.visibility.set_visible_batch(visible_up_to);
        debug!(batch_id, visible_up_to, "P₂ executing batch with L1 overlay");

        let exec_start = Instant::now();
        let (digests, applies) = manager
            .execute_batch_with_effects(pipe_batch.batch.clone())
            .await;

        let digest = digests
            .first()
            .cloned()
            .unwrap_or_else(|| pipe_batch.digest().clone());

        let mut temp_buffer = TempBuffer::new();
        applies_to_temp_buffer(&applies, &mut temp_buffer);
        let delta_bytes = temp_buffer.delta_bytes();

        let max_records = shared.pipeline.cache_config().deltapage_max_records;
        let pages = temp_buffer.into_delta_pages(batch_id, batch_id, max_records);
        shared.pipeline.on_batch_exec_complete(batch_id, pages);
        shared.visibility.set_visible_batch(batch_id);

        shared
            .pipeline
            .controller()
            .on_batch_exit(StageId::Exec, gas_weight);

        let exec_us = exec_start.elapsed().as_micros() as u64;
        shared.metrics.record_batch(StageId::Exec, exec_us);
        shared
            .metrics
            .record_workload(gas_weight, delta_bytes);
        record_backpressure_peak(&shared);

        let _ = result_tx.send((batch_id, digest.clone())).await;
        let _ = commit_tx
            .send(CommitWork {
                batch_id,
                delta_bytes,
            })
            .await;

        let _ = exec_wait_start; // stall time tracked inside wait_can_push
    }
}

async fn commit_worker(
    mut commit_rx: mpsc::Receiver<CommitWork>,
    shared: Arc<SharedPipeline>,
) {
    while let Some(work) = commit_rx.recv().await {
        wait_can_push(
            shared.pipeline.controller(),
            StageId::Commit,
            Some(&shared.metrics),
        )
        .await;
        shared
            .pipeline
            .controller()
            .on_batch_enter(StageId::Commit, work.delta_bytes);
        record_backpressure_peak(&shared);

        let cache = shared.pipeline.cache_arc();
        let batch_id = work.batch_id;
        let delta_bytes = work.delta_bytes;

        let commit_start = Instant::now();
        let commit_result =
            tokio::task::spawn_blocking(move || cache.commit_batch(batch_id)).await;

        match commit_result {
            Ok(Ok(())) => {
                debug!(batch_id, delta_bytes, "P₃ commit succeeded");
            }
            Ok(Err(e)) => {
                warn!(batch_id, error = %e, "P₃ commit failed, batch retained in L1");
            }
            Err(e) => {
                warn!(batch_id, error = %e, "P₃ commit task panicked");
            }
        }

        shared
            .pipeline
            .controller()
            .on_batch_exit(StageId::Commit, work.delta_bytes);
        shared
            .metrics
            .record_batch(StageId::Commit, commit_start.elapsed().as_micros() as u64);
        record_backpressure_peak(&shared);
    }
}

async fn wait_can_push(
    controller: &PipelineController,
    stage: StageId,
    metrics: Option<&PipelineMetrics>,
) {
    let start = Instant::now();
    let mut stalled = false;
    while !controller.can_push(stage) {
        stalled = true;
        tokio::time::sleep(Duration::from_millis(1)).await;
    }
    if stalled {
        if let Some(m) = metrics {
            m.record_stall(stage, start.elapsed().as_micros() as u64);
        }
    }
}
