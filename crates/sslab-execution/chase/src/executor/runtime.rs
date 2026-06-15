use std::sync::Arc;

use super::cache::L1Visibility;
use super::config::EvBlpConfig;
use super::pipeline::EvBlpPipeline;
use crate::executor::cache::StateStore;

/// Shared EV-BLP runtime: pipeline + L1 visibility for backend overlay.
#[derive(Clone)]
pub struct EvBlpRuntime {
    pub pipeline: Arc<EvBlpPipeline>,
    pub visibility: Arc<L1Visibility>,
}

impl EvBlpRuntime {
    pub fn new(store: Arc<dyn StateStore>) -> Self {
        let config = EvBlpConfig::from_env();
        let pipeline = Arc::new(EvBlpPipeline::from_config(config, store));
        let visibility = Arc::new(L1Visibility::new(pipeline.cache_arc()));
        Self {
            pipeline,
            visibility,
        }
    }
}
