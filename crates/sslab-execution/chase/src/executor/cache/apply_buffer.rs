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
