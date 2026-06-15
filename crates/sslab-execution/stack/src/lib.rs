//! CHASE full stack: **Tusk consensus → CDS scheduling + CHASE execution → RocksDB persistence**.
//!
//! Architecture:
//! ```text
//! Transactions → Narwhal Worker (batching)
//!       ↓
//! Tusk consensus (ordered CommittedSubDag / ConsensusOutput)
//!       ↓
//! ChaseExecutionState (adapter + parallel executor)
//!       ↓
//! PersistableCMemoryBackend (memory hot-path + RocksDB write-through)
//! ```

pub mod adapter;
pub mod execution_state;
pub mod layered_backend;
pub mod pipeline;
pub mod rocksdb_state_store;
pub mod rocksdb_store;

pub use adapter::consensus_output_to_executable_batches;
pub use execution_state::ChaseExecutionState;
pub use layered_backend::{PersistableCMemoryBackend, PersistableConcurrentEVMStorage};
pub use pipeline::ChaseStack;
pub use rocksdb_state_store::RocksDbStateStore;
pub use rocksdb_store::ChaseStorage;
