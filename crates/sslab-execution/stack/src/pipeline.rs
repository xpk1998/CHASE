use std::path::Path;
use std::sync::Arc;

use ethers_core::types::{H160, U256, U64};
use evm::backend::MemoryVicinity;
use parking_lot::RwLock;
use sslab_execution::evm_storage::EvmStorage;
use sslab_execution_chase::{CacheOverlayBackend, Chase, EvBlpConfig, EvBlpRuntime};
use tracing::info;

use crate::execution_state::ChaseExecutionState;
use crate::layered_backend::{PersistableCMemoryBackend, StackBackend, StackEvmStorage};
use crate::rocksdb_state_store::RocksDbStateStore;
use crate::rocksdb_store::ChaseStorage;

/// Builder for the full Tusk → Chase → RocksDB stack.
pub struct ChaseStack {
    pub storage: Arc<RwLock<ChaseStorage>>,
    pub evm_storage: StackEvmStorage,
    pub execution_state: ChaseExecutionState,
}

impl ChaseStack {
    /// Open (or create) RocksDB storage and wire Chase with write-through persistence.
    pub fn open(db_path: &Path, concurrency_level: usize) -> Result<Self, crate::rocksdb_store::StorageError> {
        let storage = ChaseStorage::open(db_path)?;
        let vicinity = default_vicinity();

        let ev_blp_runtime = if EvBlpConfig::is_enabled() {
            let store = Arc::new(RocksDbStateStore::new(storage.clone()));
            Some(EvBlpRuntime::new(store))
        } else {
            None
        };

        let inner_backend = PersistableCMemoryBackend::new(vicinity, storage.clone());
        let visibility = ev_blp_runtime.as_ref().map(|r| r.visibility.clone());
        let backend: StackBackend = CacheOverlayBackend::new(inner_backend, visibility);

        let evm_storage = EvmStorage::new(U64::from(9), backend, Default::default());
        let chase = Chase::new(evm_storage.clone(), concurrency_level);
        let execution_state = ChaseExecutionState::new(chase, storage.clone(), ev_blp_runtime);

        let ev_blp = EvBlpConfig::is_enabled();
        info!(
            ?db_path,
            concurrency_level,
            ev_blp,
            last_index = storage.read().last_executed_sub_dag_index().unwrap_or(0),
            "Chase stack initialized (Tusk consensus + CHASE CDS + RocksDB)"
        );

        Ok(Self {
            storage,
            evm_storage,
            execution_state,
        })
    }

    /// Use with Narwhal `Executor::spawn` and set `CHASE_USE_TUSK=1` for Tusk consensus.
    pub fn into_execution_state(self) -> ChaseExecutionState {
        self.execution_state
    }
}

fn default_vicinity() -> MemoryVicinity {
    MemoryVicinity {
        gas_price: U256::zero(),
        origin: H160::default(),
        chain_id: U256::one(),
        block_hashes: Vec::new(),
        block_number: Default::default(),
        block_coinbase: Default::default(),
        block_timestamp: Default::default(),
        block_difficulty: Default::default(),
        block_gas_limit: Default::default(),
        block_base_fee_per_gas: U256::zero(),
        block_randomness: None,
    }
}
