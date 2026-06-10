use std::sync::Arc;

use async_trait::async_trait;
use narwhal_executor::ExecutionState;
use narwhal_types::ConsensusOutput;
use parking_lot::RwLock;
use sslab_execution::executor::Executable;
use sslab_execution_chase::Chase;

use crate::layered_backend::PersistableCMemoryBackend;
use tracing::{debug, info};

use crate::adapter::consensus_output_to_executable_batches;
use crate::rocksdb_store::ChaseStorage;

/// Narwhal [`ExecutionState`] that feeds ordered consensus output into Chase
/// and persists execution progress to RocksDB.
pub struct ChaseExecutionState {
    chase: Arc<Chase<PersistableCMemoryBackend>>,
    storage: Arc<RwLock<ChaseStorage>>,
}

impl ChaseExecutionState {
    pub fn new(chase: Chase<PersistableCMemoryBackend>, storage: Arc<RwLock<ChaseStorage>>) -> Self {
        Self {
            chase: Arc::new(chase),
            storage,
        }
    }
}

#[async_trait]
impl ExecutionState for ChaseExecutionState {
    async fn handle_consensus_output(&self, consensus_output: ConsensusOutput) {
        let sub_dag_index = consensus_output.sub_dag.sub_dag_index;
        let round = consensus_output.sub_dag.leader_round();

        debug!(
            sub_dag_index,
            round,
            batch_groups = consensus_output.batches.len(),
            "Chase execution state received consensus output"
        );

        let executable_batches = consensus_output_to_executable_batches(&consensus_output);
        let batch_count = executable_batches.len();
        let tx_count: usize = executable_batches.iter().map(|b| b.data().len()).sum();

        self.chase.execute(executable_batches).await;

        if let Err(err) = self
            .storage
            .read()
            .set_last_executed_sub_dag_index(sub_dag_index)
        {
            tracing::error!(?err, sub_dag_index, "failed to persist execution index");
        }

        info!(
            sub_dag_index,
            round,
            batch_count,
            tx_count,
            "Chase executed and persisted consensus output"
        );
    }

    async fn last_executed_sub_dag_index(&self) -> u64 {
        self.storage
            .read()
            .last_executed_sub_dag_index()
            .unwrap_or(0)
    }
}
