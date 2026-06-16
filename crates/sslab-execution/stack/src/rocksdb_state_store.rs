use std::sync::Arc;

use parking_lot::RwLock;
use sslab_execution_chase::executor::cache::{
    decode_key, decode_value, encode_value, Key, StateStore, Value,
};

use crate::rocksdb_store::ChaseStorage;

/// RocksDB-backed `StateStore` for EV-BLP L2 / durable delta persistence.
pub struct RocksDbStateStore {
    storage: Arc<RwLock<ChaseStorage>>,
}

impl RocksDbStateStore {
    pub fn new(storage: Arc<RwLock<ChaseStorage>>) -> Self {
        Self { storage }
    }
}

impl StateStore for RocksDbStateStore {
    fn get(&self, key: &Key) -> Option<Value> {
        let (address, slot) = decode_key(key);
        self.storage
            .read()
            .get_storage_slot(address, slot)
            .ok()
            .flatten()
            .map(|v| encode_value(v))
    }

    fn put(&self, key: &Key, value: &Value) -> Result<(), String> {
        let (address, slot) = decode_key(key);
        let h256 = decode_value(value);
        self.storage
            .read()
            .put_storage_slot(address, slot, h256)
            .map_err(|e| e.to_string())
    }
}
