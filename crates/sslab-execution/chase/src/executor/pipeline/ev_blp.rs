use std::sync::Arc;

use super::controller::PipelineController;
use super::metrics::PipelineMetrics;
use crate::executor::cache::{StateStore, TwoLevelCache};
use crate::executor::config::{CacheConfig, EvBlpConfig, PipelineConfig};

/// EV-BLP pipeline orchestrator: P₁ Order → P₂ Exec → P₃ Commit.
pub struct EvBlpPipeline {
    controller: Arc<PipelineController>,
    cache: Arc<TwoLevelCache>,
    config: EvBlpConfig,
    metrics: Option<Arc<PipelineMetrics>>,
    next_batch_id: std::sync::atomic::AtomicU64,
    last_exec_complete: std::sync::atomic::AtomicU64,
}

impl EvBlpPipeline {
    pub fn new(
        pipeline_config: PipelineConfig,
        cache_config: CacheConfig,
        db: Arc<dyn StateStore>,
    ) -> Self {
        Self::from_config(EvBlpConfig {
            pipeline: pipeline_config,
            cache: cache_config,
        }, db)
    }

    pub fn from_config(config: EvBlpConfig, db: Arc<dyn StateStore>) -> Self {
        Self::from_config_with_metrics(config, db, None)
    }

    pub fn from_config_with_metrics(
        config: EvBlpConfig,
        db: Arc<dyn StateStore>,
        metrics: Option<Arc<PipelineMetrics>>,
    ) -> Self {
        Self {
            controller: Arc::new(PipelineController::new(&config.pipeline)),
            cache: Arc::new(TwoLevelCache::new(&config.cache, db)),
            config,
            metrics,
            next_batch_id: std::sync::atomic::AtomicU64::new(1),
            last_exec_complete: std::sync::atomic::AtomicU64::new(0),
        }
    }

    pub fn controller(&self) -> &PipelineController {
        &self.controller
    }

    pub fn cache(&self) -> &TwoLevelCache {
        &self.cache
    }

    pub fn cache_arc(&self) -> Arc<TwoLevelCache> {
        self.cache.clone()
    }

    pub fn cache_config(&self) -> &CacheConfig {
        &self.config.cache
    }

    pub fn pipeline_config(&self) -> &PipelineConfig {
        &self.config.pipeline
    }

    pub fn metrics(&self) -> Option<&PipelineMetrics> {
        self.metrics.as_deref()
    }

    pub fn metrics_arc(&self) -> Option<Arc<PipelineMetrics>> {
        self.metrics.clone()
    }

    pub fn alloc_batch_id(&self) -> u64 {
        self.next_batch_id.fetch_add(1, std::sync::atomic::Ordering::Relaxed)
    }

    pub fn completed_batch_id(&self) -> u64 {
        self.last_exec_complete.load(std::sync::atomic::Ordering::SeqCst)
    }

    /// Mark P₂ execution complete — updates L1 visibility and pipeline state.
    pub fn on_batch_exec_complete(&self, batch_id: u64, pages: Vec<super::super::cache::DeltaPage>) {
        self.cache.write_delta_pages(batch_id, pages);
        self.cache.mark_batch_complete(batch_id);
        self.last_exec_complete
            .store(batch_id, std::sync::atomic::Ordering::SeqCst);
    }
}
