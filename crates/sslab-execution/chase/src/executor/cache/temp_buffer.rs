use ethers_core::types::{H160, H256};

use super::delta_page::{encode_key, encode_value, DeltaPage, Key, Value, DELTA_PAGE_MAX_RECORDS};

/// Per-batch temporary buffer for state changes during execution.
#[derive(Default)]
pub struct TempBuffer {
    records: Vec<(Key, Value)>,
}

impl TempBuffer {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn record_write(&mut self, address: H160, slot: H256, value: H256) {
        self.records.push((encode_key(address, slot), encode_value(value)));
    }

    pub fn delta_bytes(&self) -> u64 {
        (self.records.len() as u64) * 64
    }

    /// Encode buffer into one or more DeltaPages (split at 128 records).
    pub fn into_delta_pages(
        self,
        block_start: u64,
        block_end: u64,
        max_records: usize,
    ) -> Vec<DeltaPage> {
        let page_limit = max_records.min(DELTA_PAGE_MAX_RECORDS);
        let mut pages = Vec::new();
        let mut current = DeltaPage::new(block_start, block_end);

        for (key, value) in self.records {
            if current.records.len() >= page_limit {
                current.freeze();
                pages.push(current);
                current = DeltaPage::new(block_start, block_end);
            }
            current.push_record(key, value);
        }

        if !current.records.is_empty() {
            current.freeze();
            pages.push(current);
        }

        pages
    }
}
