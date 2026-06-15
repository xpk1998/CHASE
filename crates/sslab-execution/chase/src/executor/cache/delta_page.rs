use ethers_core::types::{H160, H256};
pub type Key = [u8; 32];
pub type Value = [u8; 32];

pub const DELTA_PAGE_MAX_RECORDS: usize = 128;

/// A single state delta record within a DeltaPage.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct DeltaRecord {
    pub key: Key,
    pub value: Value,
}

/// DeltaPage holds up to 128 state delta records (~4 KB per page).
#[derive(Clone, Debug)]
pub struct DeltaPage {
    pub block_start: u64,
    pub block_end: u64,
    pub records: Vec<DeltaRecord>,
    pub is_readonly: bool,
}

impl DeltaPage {
    pub fn new(block_start: u64, block_end: u64) -> Self {
        Self {
            block_start,
            block_end,
            records: Vec::with_capacity(DELTA_PAGE_MAX_RECORDS),
            is_readonly: false,
        }
    }

    pub fn is_full(&self) -> bool {
        self.records.len() >= DELTA_PAGE_MAX_RECORDS
    }

    pub fn byte_size(&self) -> u64 {
        (self.records.len() as u64) * (64 + 8) // key + value + metadata overhead
    }

    pub fn push_record(&mut self, key: Key, value: Value) {
        debug_assert!(!self.is_readonly);
        debug_assert!(!self.is_full());
        self.records.push(DeltaRecord { key, value });
    }

    pub fn freeze(&mut self) {
        self.is_readonly = true;
    }

    pub fn lookup(&self, key: &Key) -> Option<&Value> {
        self.records
            .iter()
            .rev()
            .find(|r| &r.key == key)
            .map(|r| &r.value)
    }
}

/// Encode (address, slot) into a 32-byte key (address in first 20 bytes, slot in last 12).
pub fn encode_key(address: H160, slot: H256) -> Key {
    let mut key = [0u8; 32];
    key[..20].copy_from_slice(address.as_bytes());
    key[20..32].copy_from_slice(&slot.as_bytes()[20..32]);
    key
}

/// Encode H256 value into 32-byte value array.
pub fn encode_value(value: H256) -> Value {
    let mut v = [0u8; 32];
    v.copy_from_slice(value.as_bytes());
    v
}

/// Decode value from 32-byte array.
pub fn decode_value(bytes: &Value) -> H256 {
    H256::from_slice(bytes)
}

/// Decode a 32-byte key back to (address, slot).
pub fn decode_key(key: &Key) -> (H160, H256) {
    let mut addr_bytes = [0u8; 20];
    addr_bytes.copy_from_slice(&key[..20]);
    let mut slot = [0u8; 32];
    slot[20..32].copy_from_slice(&key[20..32]);
    (H160::from(addr_bytes), H256::from(slot))
}
