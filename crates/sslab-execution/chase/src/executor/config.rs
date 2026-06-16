use std::env;

/// EV-BLP pipeline configuration.
#[derive(Clone, Debug)]
pub struct PipelineConfig {
    /// Maximum concurrent batches per stage (ζ_max).
    pub zeta_max: u32,
    /// W₂ overload threshold (gas units).
    pub lambda2: u64,
    /// W₃ overload threshold (bytes).
    pub lambda3: u64,
}

impl Default for PipelineConfig {
    fn default() -> Self {
        Self {
            zeta_max: 8,
            lambda2: 10_000_000,
            lambda3: 64 * 1024 * 1024,
        }
    }
}

impl PipelineConfig {
    pub fn from_env() -> Self {
        let mut cfg = Self::default();
        if let Ok(v) = env::var("CHASE_PIPELINE_ZETA_MAX") {
            if let Ok(n) = v.parse() {
                cfg.zeta_max = n;
            }
        }
        if let Ok(v) = env::var("CHASE_PIPELINE_LAMBDA2") {
            if let Ok(n) = v.parse() {
                cfg.lambda2 = n;
            }
        }
        if let Ok(v) = env::var("CHASE_PIPELINE_LAMBDA3") {
            if let Ok(n) = v.parse() {
                cfg.lambda3 = n;
            }
        }
        cfg
    }
}

/// Two-level cache configuration.
#[derive(Clone, Debug)]
pub struct CacheConfig {
    /// L1 MemIndexTable capacity threshold in bytes.
    pub l1_capacity_bytes: u64,
    /// Maximum records per DeltaPage.
    pub deltapage_max_records: usize,
    /// L2 LRU cache capacity in bytes.
    pub l2_capacity_bytes: u64,
}

impl Default for CacheConfig {
    fn default() -> Self {
        Self {
            l1_capacity_bytes: 256 * 1024 * 1024,
            deltapage_max_records: 128,
            l2_capacity_bytes: 1024 * 1024 * 1024,
        }
    }
}

impl CacheConfig {
    pub fn from_env() -> Self {
        let mut cfg = Self::default();
        if let Ok(v) = env::var("CHASE_CACHE_L1_CAPACITY_MB") {
            if let Ok(mb) = v.parse::<u64>() {
                cfg.l1_capacity_bytes = mb * 1024 * 1024;
            }
        }
        if let Ok(v) = env::var("CHASE_CACHE_DELTAPAGE_MAX_RECORDS") {
            if let Ok(n) = v.parse() {
                cfg.deltapage_max_records = n;
            }
        }
        if let Ok(v) = env::var("CHASE_CACHE_L2_LRU_SIZE_MB") {
            if let Ok(mb) = v.parse::<u64>() {
                cfg.l2_capacity_bytes = mb * 1024 * 1024;
            }
        }
        cfg
    }
}

/// Combined EV-BLP configuration.
#[derive(Clone, Debug, Default)]
pub struct EvBlpConfig {
    pub pipeline: PipelineConfig,
    pub cache: CacheConfig,
}

impl EvBlpConfig {
    pub fn from_env() -> Self {
        Self {
            pipeline: PipelineConfig::from_env(),
            cache: CacheConfig::from_env(),
        }
    }

    pub fn is_enabled() -> bool {
        env::var("CHASE_USE_EV_BLP")
            .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
            .unwrap_or(false)
    }
}
