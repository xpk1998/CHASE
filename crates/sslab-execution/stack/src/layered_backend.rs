use std::sync::Arc;

use ethers_core::types::{H160, H256, U256};
use evm::backend::{Apply, Backend, Basic, MemoryVicinity};
use parking_lot::RwLock;
use sslab_execution::evm_storage::backend::{ApplyBackend, CMemoryBackend, ConcurrentHashMap};
use sslab_execution::evm_storage::EvmStorage;
use tracing::error;

use crate::rocksdb_store::ChaseStorage;

/// In-memory EVM backend with write-through persistence to RocksDB.
#[derive(Debug, Clone)]
pub struct PersistableCMemoryBackend {
    inner: CMemoryBackend,
    store: Arc<RwLock<ChaseStorage>>,
}

impl PersistableCMemoryBackend {
    pub fn new(vicinity: MemoryVicinity, store: Arc<RwLock<ChaseStorage>>) -> Self {
        let state = ConcurrentHashMap::default();
        let inner = CMemoryBackend::new(vicinity, state);

        if let Err(err) = store.read().hydrate_memory_backend(&inner) {
            error!(?err, "failed to hydrate EVM state from RocksDB; starting empty");
        }

        Self { inner, store }
    }

    pub fn memory_backend(&self) -> &CMemoryBackend {
        &self.inner
    }
}

impl Backend for PersistableCMemoryBackend {
    fn gas_price(&self) -> U256 {
        self.inner.gas_price()
    }
    fn origin(&self) -> H160 {
        self.inner.origin()
    }
    fn block_hash(&self, number: U256) -> H256 {
        self.inner.block_hash(number)
    }
    fn block_number(&self) -> U256 {
        self.inner.block_number()
    }
    fn block_coinbase(&self) -> H160 {
        self.inner.block_coinbase()
    }
    fn block_timestamp(&self) -> U256 {
        self.inner.block_timestamp()
    }
    fn block_difficulty(&self) -> U256 {
        self.inner.block_difficulty()
    }
    fn block_randomness(&self) -> Option<H256> {
        self.inner.block_randomness()
    }
    fn block_gas_limit(&self) -> U256 {
        self.inner.block_gas_limit()
    }
    fn block_base_fee_per_gas(&self) -> U256 {
        self.inner.block_base_fee_per_gas()
    }
    fn chain_id(&self) -> U256 {
        self.inner.chain_id()
    }
    fn exists(&self, address: H160) -> bool {
        self.inner.exists(address)
    }
    fn basic(&self, address: H160) -> Basic {
        self.inner.basic(address)
    }
    fn code(&self, address: H160) -> Vec<u8> {
        self.inner.code(address)
    }
    fn storage(&self, address: H160, index: H256) -> H256 {
        self.inner.storage(address, index)
    }
    fn original_storage(&self, address: H160, index: H256) -> Option<H256> {
        self.inner.original_storage(address, index)
    }
}

impl Default for PersistableCMemoryBackend {
    fn default() -> Self {
        let dir = tempfile::tempdir().expect("failed to create temp dir for default backend");
        let store = ChaseStorage::open(dir.path()).expect("failed to open default RocksDB store");
        let vicinity = MemoryVicinity {
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
        };
        Self::new(vicinity, store)
    }
}

impl ApplyBackend for PersistableCMemoryBackend {
    fn apply(&self, values: Vec<Apply>, delete_empty: bool) {
        self.inner.apply(values.clone(), delete_empty);
        if let Err(err) = self.store.read().persist_applies(values) {
            error!(?err, "failed to persist EVM state to RocksDB");
        }
    }
}

pub type PersistableConcurrentEVMStorage = EvmStorage<PersistableCMemoryBackend>;
