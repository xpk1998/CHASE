// Copyright (c) Mysten Labs, Inc.
// SPDX-License-Identifier: Apache-2.0
use std::path::{Path, PathBuf};
use std::sync::Arc;

use async_trait::async_trait;
use eyre::{Context, Result};
use executor::ExecutionState;
use sslab_execution_stack::{ChaseExecutionState, ChaseStack};
use tokio::sync::mpsc::Sender;
use tracing::info;
use types::{BatchAPI, ConsensusOutput};

/// A simple/dumb execution engine.
pub struct SimpleExecutionState {
    tx_transaction_confirmation: Sender<Vec<u8>>,
}

impl SimpleExecutionState {
    pub fn new(tx_transaction_confirmation: Sender<Vec<u8>>) -> Self {
        Self {
            tx_transaction_confirmation,
        }
    }
}

#[async_trait]
impl ExecutionState for SimpleExecutionState {
    async fn handle_consensus_output(&self, consensus_output: ConsensusOutput) {
        for batches in consensus_output.batches {
            for batch in batches {
                for transaction in batch.transactions().iter() {
                    if let Err(err) = self
                        .tx_transaction_confirmation
                        .send(transaction.clone())
                        .await
                    {
                        eprintln!("Failed to send txn in SimpleExecutionState: {}", err);
                    }
                }
            }
        }
    }

    async fn last_executed_sub_dag_index(&self) -> u64 {
        0
    }
}

/// Narwhal execution backend selected at node startup.
pub enum ConfiguredExecutionState {
    Simple(SimpleExecutionState),
    Chase(ChaseExecutionState),
}

#[async_trait]
impl ExecutionState for ConfiguredExecutionState {
    async fn handle_consensus_output(&self, consensus_output: ConsensusOutput) {
        match self {
            Self::Simple(state) => state.handle_consensus_output(consensus_output).await,
            Self::Chase(state) => state.handle_consensus_output(consensus_output).await,
        }
    }

    async fn last_executed_sub_dag_index(&self) -> u64 {
        match self {
            Self::Simple(_) => 0,
            Self::Chase(state) => state.last_executed_sub_dag_index().await,
        }
    }
}

/// Build the execution state for a primary node.
///
/// - Default: [`SimpleExecutionState`] (forwards raw transactions).
/// - `CHASE_USE_EXECUTION=1`: [`ChaseExecutionState`] with CDS scheduling and RocksDB persistence.
pub fn build_execution_state(
    store_path: &str,
    tx_transaction_confirmation: Sender<Vec<u8>>,
) -> Result<Arc<ConfiguredExecutionState>> {
    if std::env::var("CHASE_USE_EXECUTION").is_ok() {
        let chase_db = Path::new(store_path).join("chase");
        let concurrency_level = std::env::var("CHASE_CONCURRENCY_LEVEL")
            .ok()
            .and_then(|value| value.parse().ok())
            .unwrap_or(4);

        let stack = ChaseStack::open(&chase_db, concurrency_level).with_context(|| {
            format!(
                "failed to open Chase RocksDB storage at {}",
                chase_db.display()
            )
        })?;

        info!(
            db_path = %chase_db.display(),
            concurrency_level,
            "Using Chase execution with CDS scheduling"
        );

        Ok(Arc::new(ConfiguredExecutionState::Chase(
            stack.into_execution_state(),
        )))
    } else {
        Ok(Arc::new(ConfiguredExecutionState::Simple(
            SimpleExecutionState::new(tx_transaction_confirmation),
        )))
    }
}

/// Resolved RocksDB path used when `CHASE_USE_EXECUTION=1`.
pub fn chase_db_path(store_path: &str) -> PathBuf {
    Path::new(store_path).join("chase")
}
