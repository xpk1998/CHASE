use std::sync::atomic::{AtomicU64, Ordering};

use parking_lot::Mutex;

use super::stages::StageId;

/// Per-stage latency and stall counters (microseconds).
#[derive(Default)]
pub struct StageStats {
    pub batch_count: AtomicU64,
    pub total_us: AtomicU64,
    pub max_us: AtomicU64,
    pub stall_count: AtomicU64,
    pub stall_us: AtomicU64,
}

impl StageStats {
    pub fn record_batch(&self, duration_us: u64) {
        self.batch_count.fetch_add(1, Ordering::Relaxed);
        self.total_us.fetch_add(duration_us, Ordering::Relaxed);
        self.update_max(duration_us);
    }

    pub fn record_stall(&self, duration_us: u64) {
        self.stall_count.fetch_add(1, Ordering::Relaxed);
        self.stall_us.fetch_add(duration_us, Ordering::Relaxed);
    }

    fn update_max(&self, value: u64) {
        let mut current = self.max_us.load(Ordering::Relaxed);
        while value > current {
            match self.max_us.compare_exchange_weak(
                current,
                value,
                Ordering::Relaxed,
                Ordering::Relaxed,
            ) {
                Ok(_) => break,
                Err(v) => current = v,
            }
        }
    }

    pub fn snapshot(&self) -> StageStatsSnapshot {
        StageStatsSnapshot {
            batch_count: self.batch_count.load(Ordering::Relaxed),
            total_us: self.total_us.load(Ordering::Relaxed),
            max_us: self.max_us.load(Ordering::Relaxed),
            stall_count: self.stall_count.load(Ordering::Relaxed),
            stall_us: self.stall_us.load(Ordering::Relaxed),
        }
    }
}

#[derive(Clone, Debug, Default)]
pub struct StageStatsSnapshot {
    pub batch_count: u64,
    pub total_us: u64,
    pub max_us: u64,
    pub stall_count: u64,
    pub stall_us: u64,
}

impl StageStatsSnapshot {
    pub fn avg_us(&self) -> f64 {
        if self.batch_count == 0 {
            0.0
        } else {
            self.total_us as f64 / self.batch_count as f64
        }
    }
}

/// Observed per-batch workload sample for lambda calibration.
#[derive(Clone, Debug)]
pub struct WorkloadSample {
    pub gas_weight: u64,
    pub delta_bytes: u64,
}

/// EV-BLP pipeline runtime metrics.
#[derive(Default)]
pub struct PipelineMetrics {
    order: StageStats,
    exec: StageStats,
    commit: StageStats,
    samples: Mutex<Vec<WorkloadSample>>,
    max_w_exec: AtomicU64,
    max_w_commit: AtomicU64,
}

impl PipelineMetrics {
    pub fn record_batch(&self, stage: StageId, duration_us: u64) {
        self.stage_stats(stage).record_batch(duration_us);
    }

    pub fn record_stall(&self, stage: StageId, duration_us: u64) {
        self.stage_stats(stage).record_stall(duration_us);
    }

    pub fn record_workload(&self, gas_weight: u64, delta_bytes: u64) {
        self.samples.lock().push(WorkloadSample {
            gas_weight,
            delta_bytes,
        });
    }

    pub fn record_backpressure_peak(&self, w_exec: u64, w_commit: u64) {
        Self::update_max(&self.max_w_exec, w_exec);
        Self::update_max(&self.max_w_commit, w_commit);
    }

    fn update_max(cell: &AtomicU64, value: u64) {
        let mut current = cell.load(Ordering::Relaxed);
        while value > current {
            match cell.compare_exchange_weak(current, value, Ordering::Relaxed, Ordering::Relaxed)
            {
                Ok(_) => break,
                Err(v) => current = v,
            }
        }
    }

    fn stage_stats(&self, stage: StageId) -> &StageStats {
        match stage {
            StageId::Order => &self.order,
            StageId::Exec => &self.exec,
            StageId::Commit => &self.commit,
        }
    }

    pub fn workload_samples(&self) -> Vec<WorkloadSample> {
        self.samples.lock().clone()
    }

    pub fn summary(&self) -> PipelineMetricsSummary {
        PipelineMetricsSummary {
            order: self.order.snapshot(),
            exec: self.exec.snapshot(),
            commit: self.commit.snapshot(),
            max_w_exec: self.max_w_exec.load(Ordering::Relaxed),
            max_w_commit: self.max_w_commit.load(Ordering::Relaxed),
            sample_count: self.samples.lock().len() as u64,
        }
    }
}

#[derive(Clone, Debug)]
pub struct PipelineMetricsSummary {
    pub order: StageStatsSnapshot,
    pub exec: StageStatsSnapshot,
    pub commit: StageStatsSnapshot,
    pub max_w_exec: u64,
    pub max_w_commit: u64,
    pub sample_count: u64,
}

impl PipelineMetricsSummary {
    pub fn format_report(&self) -> String {
        format!(
            "EV-BLP stages (avg/max ms, stalls): \
             P1 {:.2}/{:.2} ({} stalls), \
             P2 {:.2}/{:.2} ({} stalls), \
             P3 {:.2}/{:.2} ({} stalls); \
             peak W2={} W3={} samples={}",
            self.order.avg_us() / 1000.0,
            self.order.max_us as f64 / 1000.0,
            self.order.stall_count,
            self.exec.avg_us() / 1000.0,
            self.exec.max_us as f64 / 1000.0,
            self.exec.stall_count,
            self.commit.avg_us() / 1000.0,
            self.commit.max_us as f64 / 1000.0,
            self.commit.stall_count,
            self.max_w_exec,
            self.max_w_commit,
            self.sample_count,
        )
    }
}
