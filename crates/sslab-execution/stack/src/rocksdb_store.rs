use std::path::Path;
use std::sync::Arc;

use ethers_core::types::{H160, H256, U256};
use evm::backend::{Apply, Basic};
use parking_lot::RwLock;
use serde::{Deserialize, Serialize};
use sslab_execution::evm_storage::backend::{CAccount, CMemoryBackend, ConcurrentHashMap};
use thiserror::Error;
use typed_store::rocks::{open_cf, DBMap, MetricConf, ReadWriteOptions};
use typed_store::traits::Map;
use typed_store::{reopen, TypedStoreError};

const ACCOUNTS_CF: &str = "chase_accounts";
const STORAGE_SLOTS_CF: &str = "chase_storage_slots";
const EXECUTION_INDEX_CF: &str = "chase_execution_index";
const EXECUTION_INDEX_KEY: u8 = 0;

#[derive(Debug, Error)]
pub enum StorageError {
    #[error("typed store error: {0}")]
    TypedStore(#[from] TypedStoreError),
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq, Eq)]
pub struct StoredAccount {
    pub nonce: U256,
    pub balance: U256,
    pub code: Vec<u8>,
}

/// RocksDB-backed persistence for EVM world state and execution progress.
pub struct ChaseStorage {
    accounts: DBMap<H160, StoredAccount>,
    storage_slots: DBMap<(H160, H256), H256>,
    execution_index: DBMap<u8, u64>,
}

impl std::fmt::Debug for ChaseStorage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ChaseStorage").finish()
    }
}

impl ChaseStorage {
    pub fn open(path: &Path) -> Result<Arc<RwLock<Self>>, StorageError> {
        let rocksdb = open_cf(
            path,
            None,
            MetricConf::default(),
            &[ACCOUNTS_CF, STORAGE_SLOTS_CF, EXECUTION_INDEX_CF],
        )?;
        let (accounts, storage_slots, execution_index) = reopen!(
            &rocksdb,
            ACCOUNTS_CF;<H160, StoredAccount>,
            STORAGE_SLOTS_CF;<(H160, H256), H256>,
            EXECUTION_INDEX_CF;<u8, u64>
        );
        Ok(Arc::new(RwLock::new(Self {
            accounts,
            storage_slots,
            execution_index,
        })))
    }

    pub fn last_executed_sub_dag_index(&self) -> Result<u64, StorageError> {
        Ok(self
            .execution_index
            .get(&EXECUTION_INDEX_KEY)?
            .unwrap_or(0))
    }

    pub fn set_last_executed_sub_dag_index(&self, index: u64) -> Result<(), StorageError> {
        self.execution_index
            .insert(&EXECUTION_INDEX_KEY, &index)?;
        Ok(())
    }

    /// Apply EVM state changes and persist them to RocksDB (write-through).
    pub fn persist_applies(&self, applies: Vec<Apply>) -> Result<(), StorageError> {
        for apply in applies {
            match apply {
                Apply::Modify {
                    address,
                    basic,
                    code,
                    storage,
                    reset_storage,
                } => {
                    let mut account = self
                        .accounts
                        .get(&address)?
                        .unwrap_or(StoredAccount {
                            nonce: U256::zero(),
                            balance: U256::zero(),
                            code: Vec::new(),
                        });

                    account.nonce = basic.nonce;
                    account.balance = basic.balance;
                    if let Some(code) = code {
                        account.code = code;
                    }

                    if reset_storage {
                        self.remove_storage_for_address(address)?;
                    }

                    for (slot, value) in storage {
                        if value == H256::default() {
                            let _ = self.storage_slots.remove(&(address, slot))?;
                        } else {
                            self.storage_slots.insert(&(address, slot), &value)?;
                        }
                    }

                    if account.balance.is_zero()
                        && account.nonce.is_zero()
                        && account.code.is_empty()
                    {
                        let _ = self.accounts.remove(&address)?;
                        self.remove_storage_for_address(address)?;
                    } else {
                        self.accounts.insert(&address, &account)?;
                    }
                }
                Apply::Delete { address } => {
                    let _ = self.accounts.remove(&address)?;
                    self.remove_storage_for_address(address)?;
                }
            }
        }
        Ok(())
    }

    fn remove_storage_for_address(&self, address: H160) -> Result<(), StorageError> {
        let keys = self
            .storage_slots
            .safe_iter()
            .filter_map(|entry| entry.ok())
            .filter(|((addr, _), _)| *addr == address)
            .map(|((addr, slot), _)| (addr, slot))
            .collect::<Vec<_>>();
        for key in keys {
            let _ = self.storage_slots.remove(&key)?;
        }
        Ok(())
    }

    /// Hydrate an in-memory backend from RocksDB on startup.
    pub fn hydrate_memory_backend(&self, backend: &CMemoryBackend) -> Result<(), StorageError> {
        let state = backend.state();

        for entry in self.accounts.safe_iter() {
            let (address, stored) = entry?;
            let mut storage = ConcurrentHashMap::default();
            for slot_entry in self.storage_slots.safe_iter() {
                let ((addr, slot), value) = slot_entry?;
                if addr == address {
                    storage.pin().insert(slot, value);
                }
            }

            state.pin().insert(
                address,
                CAccount {
                    nonce: stored.nonce,
                    balance: stored.balance,
                    storage,
                    code: stored.code,
                },
            );
        }

        Ok(())
    }
}
