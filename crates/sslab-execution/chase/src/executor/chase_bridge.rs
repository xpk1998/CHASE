use std::sync::Arc;
use std::time::Duration;

use narwhal_types::BatchDigest;
use sslab_execution::types::ExecutableEthereumBatch;
use tokio::sync::mpsc;
use tracing::{debug, warn};

use super::cache::{applies_to_temp_buffer, StateStore, TempBuffer};
use super::pipeline::{EvBlpPipeline, PipelineBatch, PipelineController, StageId};
use super::runtime::EvBlpRuntime;
use crate::chase_core::ConcurrencyLevelManager;

/// Shared pipeline state for concurrent stage workers.
pub struct SharedPipeline {
    pub pipeline: Arc<EvBlpPipeline>,
    pub visibility: Arc<super::cache::L1Visibility>,
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

    pub async fn execute(&self, batches: Vec<ExecutableEthereumBatch>) -> Vec<BatchDigest> {
        let n = batches.len();
        if n == 0 {
            return vec![];
        }

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
            wait_can_push(self.shared.pipeline.controller(), StageId::Order).await;
            let batch_id = self.shared.pipeline.alloc_batch_id();
            let pipe_batch = PipelineBatch::new(batch_id, batch);
            self.shared
                .pipeline
                .controller()
                .on_batch_enter(StageId::Order, pipe_batch.gas_weight);
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

        results.into_iter().map(|(_, d)| d).collect()
    }
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

        wait_can_push(shared.pipeline.controller(), StageId::Exec).await;
        shared
            .pipeline
            .controller()
            .on_batch_enter(StageId::Exec, gas_weight);

        let visible_up_to = shared.pipeline.completed_batch_id();
        shared.visibility.set_visible_batch(visible_up_to);
        debug!(batch_id, visible_up_to, "P₂ executing batch with L1 overlay");

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

        let _ = result_tx.send((batch_id, digest.clone())).await;
        let _ = commit_tx
            .send(CommitWork {
                batch_id,
                delta_bytes,
            })
            .await;
    }
}

async fn commit_worker(
    mut commit_rx: mpsc::Receiver<CommitWork>,
    shared: Arc<SharedPipeline>,
) {
    while let Some(work) = commit_rx.recv().await {
        wait_can_push(shared.pipeline.controller(), StageId::Commit).await;
        shared
            .pipeline
            .controller()
            .on_batch_enter(StageId::Commit, work.delta_bytes);

        let cache = shared.pipeline.cache_arc();
        let batch_id = work.batch_id;
        let delta_bytes = work.delta_bytes;

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
    }
}

async fn wait_can_push(controller: &PipelineController, stage: StageId) {
    while !controller.can_push(stage) {
        tokio::time::sleep(Duration::from_millis(1)).await;
    }
}
