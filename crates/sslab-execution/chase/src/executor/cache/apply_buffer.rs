use ethers_core::types::H256;
use evm::backend::Apply;

use super::temp_buffer::TempBuffer;

/// Convert EVM `Apply` effects into a per-batch `TempBuffer`.
pub fn applies_to_temp_buffer(applies: &[Apply], buffer: &mut TempBuffer) {
    for apply in applies {
        match apply {
            Apply::Modify {
                address,
                storage,
                ..
            } => {
                for (slot, value) in storage {
                    buffer.record_write(*address, *slot, *value);
                }
            }
            Apply::Delete { .. } => {}
        }
    }
}

/// Estimate delta bytes from applies (storage slots only).
pub fn applies_delta_bytes(applies: &[Apply]) -> u64 {
    let mut count = 0u64;
    for apply in applies {
        if let Apply::Modify { storage, .. } = apply {
            count += storage.len() as u64;
        }
    }
    count * 64
}

/// Encode account balance/nonce changes using a sentinel slot (all 0xFF).
pub fn encode_account_slot() -> H256 {
    H256::from([0xFFu8; 32])
}
