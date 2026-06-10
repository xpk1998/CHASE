//! Seer-accelerated pre-execution for Chase.
//!
//! Integrates ideas from [Seer (VLDB 2025)](https://www.vldb.org/pvldb/vol18/p822-xiao.pdf):
//! - **VarTable + Perceptron**: two-level branch-direction learning across transactions
//! - **PreExecutionCache**: checkpoint-style reuse of prior simulation RW sets / effects
//! - **Contract-locality ordering**: warm the predictor before parallel simulation
//!
//! Full fine-grained EVM branch prediction (JUMPI hooks) lives in the SeerEVM Go
//! implementation; this Rust layer accelerates Chase's simulation phase and maintains
//! predictor state for future chase-evm interpreter integration.

mod context;
mod perceptron;
mod pre_execution_cache;
mod simulator;
mod var_table;

pub use context::{SeerConfig, SeerContext};
pub use perceptron::{bool_to_branch_res, Perceptron, NOT_TAKEN, TAKEN, UNCERTAIN};
pub use pre_execution_cache::{CachedSimulation, PreExecutionCache, SharedPreExecutionCache};
pub use simulator::{order_for_contract_locality, seer_simulate_batch, seer_simulate_tx};
pub use var_table::{SharedVarTable, VarTable};

#[cfg(test)]
mod tests;
