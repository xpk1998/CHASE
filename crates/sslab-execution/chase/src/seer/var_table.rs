//! Two-level branch history table for state-variable branches (Seer VarTable).

use ethers_core::types::{Address, H256};
use hashbrown::HashMap;
use parking_lot::RwLock;

use super::perceptron::{bool_to_branch_res, Perceptron, TAKEN, UNCERTAIN};

#[derive(Debug, Clone)]
struct BranchInfo {
    perceptron: Perceptron,
    regular: bool,
}

impl BranchInfo {
    fn new() -> Self {
        Self {
            perceptron: Perceptron::new(),
            regular: false,
        }
    }
}

#[derive(Debug, Default)]
struct SlotEntry {
    branches: HashMap<String, BranchInfo>,
}

#[derive(Debug, Default)]
struct ContractSubTable {
    slots: HashMap<H256, SlotEntry>,
}

/// Global branch predictor table keyed by contract address and storage slot.
#[derive(Debug, Default)]
pub struct VarTable {
    contracts: HashMap<Address, ContractSubTable>,
}

impl VarTable {
    pub fn predict(&mut self, contract: Address, slot: H256, branch_id: &str) -> i32 {
        let Some(entry) = self
            .contracts
            .get_mut(&contract)
            .and_then(|sub| sub.slots.get_mut(&slot))
        else {
            return UNCERTAIN;
        };

        let Some(branch) = entry.branches.get_mut(branch_id) else {
            return UNCERTAIN;
        };

        let res = branch.perceptron.predict(false);
        branch.perceptron.push_last_pred(res);
        branch.regular = matches!(res, TAKEN | super::perceptron::NOT_TAKEN);
        res
    }

    pub fn update(&mut self, contract: Address, slot: H256, branch_id: &str, taken: bool) {
        let sub = self.contracts.entry(contract).or_default();
        let entry = sub.slots.entry(slot).or_default();
        let branch = entry.branches.entry(branch_id.to_string()).or_insert_with(BranchInfo::new);
        let dir = bool_to_branch_res(taken);
        let pred = if branch.perceptron.has_last_prediction() {
            branch.perceptron.last_prediction().unwrap()
        } else {
            branch.perceptron.predict(true)
        };
        branch.perceptron.update(dir, pred);
    }

    pub fn ensure_branch(&mut self, contract: Address, slot: H256, branch_id: &str) {
        let sub = self.contracts.entry(contract).or_default();
        let entry = sub.slots.entry(slot).or_default();
        entry.branches.entry(branch_id.to_string()).or_insert_with(BranchInfo::new);
    }

    /// Infer branch outcomes from observed read/write keys (simplified pre-EVM-hook learning).
    pub fn learn_from_rw_access(
        &mut self,
        contract: Address,
        read_keys: &hashbrown::HashSet<H256>,
        write_keys: &hashbrown::HashSet<H256>,
    ) {
        for slot in read_keys.iter().chain(write_keys.iter()) {
            let branch_id = format!("rw:{slot:?}");
            self.ensure_branch(contract, *slot, &branch_id);
            let taken = write_keys.contains(slot);
            self.update(contract, *slot, &branch_id, taken);
        }
    }
}

/// Thread-safe wrapper used during parallel simulation.
#[derive(Debug, Default)]
pub struct SharedVarTable {
    inner: RwLock<VarTable>,
}

impl SharedVarTable {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn predict(&self, contract: Address, slot: H256, branch_id: &str) -> i32 {
        self.inner.write().predict(contract, slot, branch_id)
    }

    pub fn learn_from_rw_access(
        &self,
        contract: Address,
        read_keys: &hashbrown::HashSet<H256>,
        write_keys: &hashbrown::HashSet<H256>,
    ) {
        self.inner
            .write()
            .learn_from_rw_access(contract, read_keys, write_keys);
    }
}
