use std::sync::Arc;

use super::cache::L1Visibility;
use super::config::EvBlpConfig;
use super::pipeline::{EvBlpPipeline, PipelineMetrics};
use crate::executor::cache::StateStore;

/// Shared EV-BLP runtime: pipeline + L1 visibility for backend overlay.
#[derive(Clone)]
pub struct EvBlpRuntime {
    pub pipeline: Arc<EvBlpPipeline>,
    pub visibility: Arc<super::cache::L1Visibility>,
    pub metrics: Arc<PipelineMetrics>,
}

impl EvBlpRuntime {
    pub fn new(store: Arc<dyn StateStore>) -> Self {
        let config = EvBlpConfig::from_env();
        let metrics = Arc::new(PipelineMetrics::default());
        let pipeline = Arc::new(EvBlpPipeline::from_config_with_metrics(
            config,
            store,
            Some(metrics.clone()),
        ));
        let visibility = Arc::new(L1Visibility::new(pipeline.cache_arc()));
        Self {
            pipeline,
            visibility,
            metrics,
        }
    }
}
