use ethers_core::types::{H160, H256};

use super::delta_page::{encode_key, encode_value, DeltaPage, DELTA_PAGE_MAX_RECORDS};
use super::temp_buffer::TempBuffer;
use super::two_level_cache::{InMemoryStateStore, StateStore, TwoLevelCache};
use crate::executor::config::CacheConfig;

#[test]
fn delta_page_holds_up_to_128_records() {
    let mut page = DeltaPage::new(0, 0);
    for i in 0..DELTA_PAGE_MAX_RECORDS {
        let key = encode_key(H160::from_low_u64_be(i as u64), H256::zero());
        let val = encode_value(H256::from_low_u64_be(i as u64 + 1));
        page.push_record(key, val);
    }
    assert!(page.is_full());
    page.freeze();
    assert!(page.is_readonly);
}

#[test]
fn temp_buffer_splits_into_multiple_pages() {
    let mut buf = TempBuffer::new();
    for i in 0..200 {
        buf.record_write(
            H160::from_low_u64_be(i),
            H256::from_low_u64_be(i),
            H256::from_low_u64_be(i + 1),
        );
    }
    let pages = buf.into_delta_pages(1, 1, DELTA_PAGE_MAX_RECORDS);
    assert_eq!(pages.len(), 2);
    assert_eq!(pages[0].records.len(), DELTA_PAGE_MAX_RECORDS);
    assert_eq!(pages[1].records.len(), 200 - DELTA_PAGE_MAX_RECORDS);
}

#[test]
fn two_level_cache_l1_visibility() {
    let config = CacheConfig::default();
    let store = std::sync::Arc::new(InMemoryStateStore::default());
    let cache = TwoLevelCache::new(&config, store);

    let key = encode_key(H160::from_low_u64_be(1), H256::from_low_u64_be(2));
    let val = encode_value(H256::from_low_u64_be(42));

    let mut page = DeltaPage::new(1, 1);
    page.push_record(key, val);
    page.freeze();

    cache.write_delta_pages(1, vec![page]);
    assert!(cache.read(&key, 1).is_none());

    cache.mark_batch_complete(1);
    assert_eq!(cache.read(&key, 1), Some(val));
}

#[test]
fn two_level_cache_read_through_l2_and_db() {
    let config = CacheConfig::default();
    let store = std::sync::Arc::new(InMemoryStateStore::default());
    let cache = TwoLevelCache::new(&config, store.clone());

    let key = encode_key(H160::from_low_u64_be(99), H256::zero());
    let val = encode_value(H256::from_low_u64_be(7));

    store.put(&key, &val).unwrap();
    cache.mark_batch_complete(0);
    assert_eq!(cache.read(&key, 0), Some(val));
}
