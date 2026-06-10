//! Shared Seer context for a batch of transaction simulations.

use std::sync::Arc;

use super::pre_execution_cache::SharedPreExecutionCache;
use super::var_table::SharedVarTable;

/// Configuration for Seer-accelerated simulation.
#[derive(Debug, Clone)]
pub struct SeerConfig {
    /// Enable perceptron-based branch table learning.
    pub enable_perceptron: bool,
    /// Enable pre-execution result caching (checkpoint fast-path).
    pub enable_cache: bool,
    /// Sort txs by target contract to warm the branch predictor.
    pub contract_locality_ordering: bool,
}

impl Default for SeerConfig {
    fn default() -> Self {
        Self {
            enable_perceptron: true,
            enable_cache: true,
            contract_locality_ordering: true,
        }
    }
}

impl SeerConfig {
    pub fn from_env() -> Self {
        let enabled = std::env::var("CHASE_USE_SEER")
            .map(|v| v != "0" && v != "false")
            .unwrap_or(true);

        if !enabled {
            return Self {
                enable_perceptron: false,
                enable_cache: false,
                contract_locality_ordering: false,
            };
        }

        Self {
            enable_perceptron: std::env::var("CHASE_SEER_PERCEPTRON")
                .map(|v| v != "0" && v != "false")
                .unwrap_or(true),
            enable_cache: std::env::var("CHASE_SEER_CACHE")
                .map(|v| v != "0" && v != "false")
                .unwrap_or(true),
            contract_locality_ordering: std::env::var("CHASE_SEER_LOCALITY")
                .map(|v| v != "0" && v != "false")
                .unwrap_or(true),
        }
    }

    pub fn is_enabled(&self) -> bool {
        self.enable_perceptron || self.enable_cache || self.contract_locality_ordering
    }
}

/// Cross-transaction Seer state reused during Chase simulation.
#[derive(Debug, Clone)]
pub struct SeerContext {
    pub config: SeerConfig,
    pub var_table: Arc<SharedVarTable>,
    pub pre_execution_cache: Arc<SharedPreExecutionCache>,
}

impl SeerContext {
    pub fn new(config: SeerConfig) -> Self {
        Self {
            config,
            var_table: Arc::new(SharedVarTable::new()),
            pre_execution_cache: Arc::new(SharedPreExecutionCache::new()),
        }
    }

    pub fn from_env() -> Self {
        Self::new(SeerConfig::from_env())
    }

    pub fn disabled() -> Self {
        Self::new(SeerConfig {
            enable_perceptron: false,
            enable_cache: false,
            contract_locality_ordering: false,
        })
    }

    pub fn cache_stats(&self) -> (u64, u64) {
        self.pre_execution_cache.stats()
    }
}
