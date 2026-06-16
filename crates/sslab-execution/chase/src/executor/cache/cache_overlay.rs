use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;

use ethers_core::types::{H160, H256, U256};
use evm::backend::{Apply, Backend, Basic};

use super::delta_page::{decode_value, encode_key};
use super::two_level_cache::TwoLevelCache;
use sslab_execution::evm_storage::backend::ApplyBackend;

/// Shared L1 visibility context between the pipeline and EVM backend overlay.
pub struct L1Visibility {
    cache: Arc<TwoLevelCache>,
    visible_batch: AtomicU64,
}

impl L1Visibility {
    pub fn new(cache: Arc<TwoLevelCache>) -> Self {
        Self {
            cache,
            visible_batch: AtomicU64::new(0),
        }
    }

    pub fn cache(&self) -> &TwoLevelCache {
        &self.cache
    }

    pub fn set_visible_batch(&self, batch_id: u64) {
        self.visible_batch.store(batch_id, Ordering::SeqCst);
    }

    pub fn visible_batch(&self) -> u64 {
        self.visible_batch.load(Ordering::SeqCst)
    }
}

/// EVM backend wrapper that reads uncommitted L1 state from `TwoLevelCache`.
#[derive(Clone)]
pub struct CacheOverlayBackend<B> {
    inner: B,
    visibility: Option<Arc<L1Visibility>>,
}

impl<B> CacheOverlayBackend<B> {
    pub fn new(inner: B, visibility: Option<Arc<L1Visibility>>) -> Self {
        Self { inner, visibility }
    }

    pub fn inner(&self) -> &B {
        &self.inner
    }

    pub fn visibility(&self) -> Option<&Arc<L1Visibility>> {
        self.visibility.as_ref()
    }
}

impl<B: Default> Default for CacheOverlayBackend<B> {
    fn default() -> Self {
        Self::new(B::default(), None)
    }
}

impl<B: Backend> Backend for CacheOverlayBackend<B> {
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
        if let Some(vis) = &self.visibility {
            let key = encode_key(address, index);
            let visible = vis.visible_batch();
            if let Some(val) = vis.cache().read(&key, visible) {
                return decode_value(&val);
            }
        }
        self.inner.storage(address, index)
    }

    fn original_storage(&self, address: H160, index: H256) -> Option<H256> {
        Some(self.storage(address, index))
    }
}

impl<B: ApplyBackend> ApplyBackend for CacheOverlayBackend<B> {
    fn apply(&self, values: Vec<Apply>, delete_empty: bool) {
        self.inner.apply(values, delete_empty);
    }
}

#[cfg(test)]
mod tests {
    use std::sync::Arc;

    use ethers_core::types::{H160, H256};
    use evm::backend::Backend;

    use super::{CacheOverlayBackend, L1Visibility};
    use crate::executor::cache::{
        temp_buffer::TempBuffer,
        InMemoryStateStore, TwoLevelCache,
    };
    use crate::executor::config::CacheConfig;
    use sslab_execution::evm_storage::backend::CMemoryBackend;

    #[test]
    fn overlay_reads_uncommitted_l1_state() {
        let inner = CMemoryBackend::default();
        let config = CacheConfig::default();
        let store = Arc::new(InMemoryStateStore::default());
        let cache = Arc::new(TwoLevelCache::new(&config, store));
        let visibility = Arc::new(L1Visibility::new(cache));

        let overlay = CacheOverlayBackend::new(inner, Some(visibility.clone()));

        let addr = H160::from_low_u64_be(1);
        let slot = H256::from_low_u64_be(2);
        let val = H256::from_low_u64_be(99);

        let mut buf = TempBuffer::new();
        buf.record_write(addr, slot, val);
        let pages = buf.into_delta_pages(1, 1, 128);
        visibility.cache().write_delta_pages(1, pages);
        visibility.cache().mark_batch_complete(1);
        visibility.set_visible_batch(1);

        assert_eq!(overlay.storage(addr, slot), val);
        assert_eq!(
            overlay.inner().storage(addr, slot),
            H256::default(),
            "inner backend should not see uncommitted state"
        );
    }

    #[test]
    fn overlay_passthrough_without_visibility() {
        let inner = CMemoryBackend::default();
        let overlay = CacheOverlayBackend::<CMemoryBackend>::new(inner, None);
        let addr = H160::from_low_u64_be(5);
        assert_eq!(overlay.storage(addr, H256::zero()), H256::default());
    }
}
