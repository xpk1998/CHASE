use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;

use parking_lot::RwLock;
use tracing::warn;

use super::delta_page::{decode_value, DeltaPage, Key, Value};
use super::l1_cache::L1Cache;
use super::l2_cache::L2Cache;
use crate::executor::config::CacheConfig;

/// Trait for persistent state storage (RocksDB backend).
pub trait StateStore: Send + Sync {
    fn get(&self, key: &Key) -> Option<Value>;
    fn put(&self, key: &Key, value: &Value) -> Result<(), String>;
    fn put_batch(&self, entries: &[(Key, Value)]) -> Result<(), String> {
        for (k, v) in entries {
            self.put(k, v)?;
        }
        Ok(())
    }
}

/// In-memory StateStore for testing.
#[derive(Default)]
pub struct InMemoryStateStore {
    data: RwLock<hashbrown::HashMap<Key, Value>>,
}

impl StateStore for InMemoryStateStore {
    fn get(&self, key: &Key) -> Option<Value> {
        self.data.read().get(key).copied()
    }

    fn put(&self, key: &Key, value: &Value) -> Result<(), String> {
        self.data.write().insert(*key, *value);
        Ok(())
    }
}

/// Two-level cache: L1 (memory, uncommitted) + L2 (LRU, committed) + persistent store.
pub struct TwoLevelCache {
    l1: L1Cache,
    l2: L2Cache,
    db: Arc<dyn StateStore>,
    /// Highest batch_id whose execution is complete and visible to subsequent batches.
    completed_batch: AtomicU64,
    /// Per-batch delta byte counts for W₃ tracking.
    batch_delta_bytes: RwLock<hashbrown::HashMap<u64, u64>>,
}

impl TwoLevelCache {
    pub fn new(config: &CacheConfig, db: Arc<dyn StateStore>) -> Self {
        Self {
            l1: L1Cache::new(config.l1_capacity_bytes),
            l2: L2Cache::new(config.l2_capacity_bytes),
            db,
            completed_batch: AtomicU64::new(0),
            batch_delta_bytes: RwLock::new(hashbrown::HashMap::new()),
        }
    }

    /// Write DeltaPages from a completed batch execution.
    pub fn write_delta_pages(&self, batch_id: u64, pages: Vec<DeltaPage>) {
        let mut total_bytes = 0u64;
        for page in pages {
            total_bytes += page.byte_size();
            self.l1.write_delta_page(page);
        }
        self.batch_delta_bytes
            .write()
            .insert(batch_id, total_bytes);
    }

    /// Mark batch execution complete — state becomes visible to subsequent batches.
    pub fn mark_batch_complete(&self, batch_id: u64) {
        let prev = self.completed_batch.fetch_max(batch_id, Ordering::SeqCst);
        if batch_id > prev {
            tracing::debug!(batch_id, "batch execution marked complete in L1 cache");
        }
    }

    pub fn completed_batch_id(&self) -> u64 {
        self.completed_batch.load(Ordering::SeqCst)
    }

    /// Read state visible up to `visible_up_to` batch.
    pub fn read(&self, key: &Key, visible_up_to: u64) -> Option<Value> {
        let completed = self.completed_batch.load(Ordering::SeqCst);
        if visible_up_to > completed {
            return None;
        }

        if let Some(v) = self.l1.read(key) {
            return Some(v);
        }

        if let Some(v) = self.l2.get(key) {
            return Some(v);
        }

        self.db.get(key)
    }

    pub fn delta_bytes_for_batch(&self, batch_id: u64) -> u64 {
        self.batch_delta_bytes
            .read()
            .get(&batch_id)
            .copied()
            .unwrap_or(0)
    }

    /// Async flush frozen L1 tables to L2 + persistent storage.
    pub fn flush_frozen_tables(&self) -> Result<usize, String> {
        let frozen = self.l1.take_frozen_tables();
        let mut flushed = 0;

        for table in &frozen {
            let pages = table.drain_pages();
            for page in pages {
                let entries: Vec<(Key, Value)> = page
                    .records
                    .iter()
                    .map(|r| (r.key, r.value))
                    .collect();

                self.l2.insert_batch(entries.iter().copied());
                self.db.put_batch(&entries)?;
                flushed += 1;
            }
        }

        Ok(flushed)
    }

    /// Retry flush on failure — frozen tables are NOT removed from L1 on failure.
    pub fn try_flush_with_retry(&self, max_retries: u32) -> Result<usize, String> {
        let mut last_err = String::new();
        for attempt in 0..max_retries {
            match self.flush_frozen_tables() {
                Ok(n) => return Ok(n),
                Err(e) => {
                    warn!(attempt, error = %e, "L1 flush failed, will retry");
                    last_err = e;
                }
            }
        }
        Err(format!(
            "flush failed after {} retries: {}",
            max_retries, last_err
        ))
    }

    pub fn l1(&self) -> &L1Cache {
        &self.l1
    }

    pub fn l2(&self) -> &L2Cache {
        &self.l2
    }
}

/// Helper to read a decoded H256 value.
pub fn read_h256(cache: &TwoLevelCache, key: &Key, visible_up_to: u64) -> Option<ethers_core::types::H256> {
    cache.read(key, visible_up_to).map(|v| decode_value(&v))
}
