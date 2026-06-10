//! Pre-execution result cache (Seer PreExecutionTable analogue).

use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

use ethers_core::types::Address;
use evm::backend::{Apply, Log};
use evm::executor::stack::RwSet;
use hashbrown::HashMap;
use parking_lot::RwLock;
use sslab_execution::types::EthereumTransaction;

#[derive(Clone, Debug)]
pub struct CachedSimulation {
    pub effects: Vec<Apply>,
    pub logs: Vec<Log>,
    pub rw_set: RwSet,
}

#[derive(Hash, PartialEq, Eq, Clone, Debug)]
struct TxFingerprint {
    caller: Address,
    to: Option<Address>,
    value_hash: u64,
    data_hash: u64,
    gas_limit: u64,
}

impl TxFingerprint {
    fn from_tx(tx: &EthereumTransaction) -> Self {
        let value_hash = {
            let mut h = DefaultHasher::new();
            tx.value().0.hash(&mut h);
            h.finish()
        };
        let data_hash = {
            let mut h = DefaultHasher::new();
            if let Some(data) = tx.data() {
                data.hash(&mut h);
            }
            h.finish()
        };

        Self {
            caller: tx.caller(),
            to: tx.to_addr().copied(),
            value_hash,
            data_hash,
            gas_limit: tx.gas_limit(),
        }
    }
}

/// Cache of prior simulation results for fast-path reuse.
#[derive(Debug, Default)]
pub struct PreExecutionCache {
    entries: HashMap<TxFingerprint, CachedSimulation>,
    hits: u64,
    misses: u64,
}

impl PreExecutionCache {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn get(&mut self, tx: &EthereumTransaction) -> Option<CachedSimulation> {
        let key = TxFingerprint::from_tx(tx);
        if let Some(cached) = self.entries.get(&key).cloned() {
            self.hits += 1;
            Some(cached)
        } else {
            self.misses += 1;
            None
        }
    }

    pub fn insert(&mut self, tx: &EthereumTransaction, cached: CachedSimulation) {
        let key = TxFingerprint::from_tx(tx);
        self.entries.insert(key, cached);
    }

    pub fn stats(&self) -> (u64, u64) {
        (self.hits, self.misses)
    }
}

#[derive(Debug, Default)]
pub struct SharedPreExecutionCache {
    inner: RwLock<PreExecutionCache>,
}

impl SharedPreExecutionCache {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn get(&self, tx: &EthereumTransaction) -> Option<CachedSimulation> {
        self.inner.write().get(tx)
    }

    pub fn insert(&self, tx: &EthereumTransaction, cached: CachedSimulation) {
        self.inner.write().insert(tx, cached);
    }

    pub fn stats(&self) -> (u64, u64) {
        self.inner.read().stats()
    }
}
