use std::sync::atomic::{AtomicU64, Ordering};

use parking_lot::RwLock;

use super::delta_page::{DeltaPage, Key};

/// In-memory index table mapping keys to their latest DeltaPage.
pub struct MemIndexTable {
    index: RwLock<hashbrown::HashMap<Key, usize>>,
    pages: RwLock<Vec<DeltaPage>>,
    total_bytes: AtomicU64,
    frozen: RwLock<bool>,
    capacity_threshold: u64,
}

impl MemIndexTable {
    pub fn new(capacity_threshold: u64) -> Self {
        Self {
            index: RwLock::new(hashbrown::HashMap::new()),
            pages: RwLock::new(Vec::new()),
            total_bytes: AtomicU64::new(0),
            frozen: RwLock::new(false),
            capacity_threshold,
        }
    }

    pub fn total_bytes(&self) -> u64 {
        self.total_bytes.load(Ordering::Relaxed)
    }

    pub fn is_frozen(&self) -> bool {
        *self.frozen.read()
    }

    pub fn is_at_capacity(&self) -> bool {
        self.total_bytes() >= self.capacity_threshold
    }

    /// Insert a readonly DeltaPage into this table.
    pub fn insert_page(&self, page: DeltaPage) {
        assert!(page.is_readonly, "DeltaPage must be readonly before insertion");
        let page_size = page.byte_size();
        let page_idx = {
            let mut pages = self.pages.write();
            let idx = pages.len();
            pages.push(page);
            idx
        };

        let mut index = self.index.write();
        if let Some(page) = self.pages.read().get(page_idx) {
            for record in &page.records {
                index.insert(record.key, page_idx);
            }
        }

        self.total_bytes.fetch_add(page_size, Ordering::Relaxed);
    }

    pub fn lookup(&self, key: &Key) -> Option<super::delta_page::Value> {
        let index = self.index.read();
        let page_idx = index.get(key)?;
        let pages = self.pages.read();
        let page = pages.get(*page_idx)?;
        page.lookup(key).copied()
    }

    pub fn freeze(&self) {
        *self.frozen.write() = true;
    }

    /// Drain all pages for async flush. Only valid on frozen tables.
    pub fn drain_pages(&self) -> Vec<DeltaPage> {
        assert!(self.is_frozen(), "can only drain frozen tables");
        let mut pages = self.pages.write();
        let drained = std::mem::take(&mut *pages);
        self.index.write().clear();
        self.total_bytes.store(0, Ordering::Relaxed);
        drained
    }
}

impl std::fmt::Debug for MemIndexTable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("MemIndexTable")
            .field("total_bytes", &self.total_bytes())
            .field("frozen", &self.is_frozen())
            .finish()
    }
}
