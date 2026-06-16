use std::num::NonZeroUsize;

use lru::LruCache;
use parking_lot::Mutex;

use super::delta_page::{Key, Value};

/// L2 LRU cache for committed index entries.
pub struct L2Cache {
    cache: Mutex<LruCache<Key, Value>>,
    capacity_bytes: u64,
    current_bytes: Mutex<u64>,
}

impl L2Cache {
    pub fn new(capacity_bytes: u64) -> Self {
        let cap = NonZeroUsize::new(1024).unwrap();
        Self {
            cache: Mutex::new(LruCache::new(cap)),
            capacity_bytes,
            current_bytes: Mutex::new(0),
        }
    }

    pub fn get(&self, key: &Key) -> Option<Value> {
        self.cache.lock().get(key).copied()
    }

    pub fn insert(&self, key: Key, value: Value) {
        let entry_size = 64u64;
        let mut cache = self.cache.lock();
        let mut current = self.current_bytes.lock();

        if let Some(old) = cache.put(key, value) {
            // replaced existing entry, no size change
            let _ = old;
        } else {
            *current += entry_size;
        }

        while *current > self.capacity_bytes && cache.len() > 1 {
            if cache.pop_lru().is_some() {
                *current = current.saturating_sub(entry_size);
            }
        }
    }

    pub fn insert_batch(&self, entries: impl IntoIterator<Item = (Key, Value)>) {
        for (key, value) in entries {
            self.insert(key, value);
        }
    }
}

impl std::fmt::Debug for L2Cache {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("L2Cache")
            .field("current_bytes", &*self.current_bytes.lock())
            .finish()
    }
}
