use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;

use parking_lot::RwLock;

use super::delta_page::{DeltaPage, Key, Value};
use super::mem_index_table::MemIndexTable;

/// L1 cache: collection of active and frozen MemIndexTables.
pub struct L1Cache {
    active: RwLock<Arc<MemIndexTable>>,
    frozen_tables: RwLock<Vec<Arc<MemIndexTable>>>,
    capacity_threshold: u64,
    /// Monotonically increasing table id for ordering (newer = higher).
    next_table_id: AtomicU64,
    table_order: RwLock<Vec<(u64, Arc<MemIndexTable>)>>,
}

impl L1Cache {
    pub fn new(capacity_threshold: u64) -> Self {
        Self {
            active: RwLock::new(Arc::new(MemIndexTable::new(capacity_threshold))),
            frozen_tables: RwLock::new(Vec::new()),
            capacity_threshold,
            next_table_id: AtomicU64::new(0),
            table_order: RwLock::new(Vec::new()),
        }
    }

    pub fn write_delta_page(&self, page: DeltaPage) {
        let active = self.active.read().clone();
        active.insert_page(page);

        if active.is_at_capacity() {
            self.freeze_active();
        }
    }

    fn freeze_active(&self) {
        let old_active = {
            let mut active_guard = self.active.write();
            let old = active_guard.clone();
            old.freeze();
            *active_guard = Arc::new(MemIndexTable::new(self.capacity_threshold));
            old
        };

        let table_id = self.next_table_id.fetch_add(1, Ordering::Relaxed);
        self.frozen_tables.write().push(old_active.clone());
        self.table_order.write().push((table_id, old_active));
    }

    /// Take frozen tables pending async flush.
    pub fn take_frozen_tables(&self) -> Vec<Arc<MemIndexTable>> {
        let tables: Vec<_> = self.frozen_tables.write().drain(..).collect();
        self.table_order.write().retain(|(_, t)| {
            !tables.iter().any(|ft| Arc::ptr_eq(ft, t))
        });
        tables
    }

    /// Read from L1: search active + frozen tables from newest to oldest.
    pub fn read(&self, key: &Key) -> Option<Value> {
        if let Some(v) = self.active.read().lookup(key) {
            return Some(v);
        }

        let order = self.table_order.read();
        for (_, table) in order.iter().rev() {
            if let Some(v) = table.lookup(key) {
                return Some(v);
            }
        }
        None
    }

    pub fn active_table(&self) -> Arc<MemIndexTable> {
        self.active.read().clone()
    }
}

impl std::fmt::Debug for L1Cache {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("L1Cache")
            .field("frozen_count", &self.frozen_tables.read().len())
            .finish()
    }
}
