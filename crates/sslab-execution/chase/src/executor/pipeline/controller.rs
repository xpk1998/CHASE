use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};

use parking_lot::Mutex;

use super::stages::{StageId, STAGE_COUNT};
use crate::executor::config::PipelineConfig;

/// AIMD-based pipeline window controller for EV-BLP.
///
/// Controls per-stage window sizes (ζ_i) based on downstream backpressure
/// weights (W_{i+1}).
pub struct PipelineController {
    zeta: [AtomicU32; STAGE_COUNT],
    w: [AtomicU64; STAGE_COUNT],
    lambda: [AtomicU64; STAGE_COUNT],
    zeta_max: u32,
    /// Number of batches currently in each stage.
    in_flight: Mutex<[u32; STAGE_COUNT]>,
}

impl PipelineController {
    pub fn new(config: &PipelineConfig) -> Self {
        let lambda = [
            AtomicU64::new(0), // W[0] unused
            AtomicU64::new(config.lambda2),
            AtomicU64::new(config.lambda3),
        ];
        Self {
            zeta: [
                AtomicU32::new(1),
                AtomicU32::new(1),
                AtomicU32::new(1),
            ],
            w: [
                AtomicU64::new(0),
                AtomicU64::new(0),
                AtomicU64::new(0),
            ],
            lambda,
            zeta_max: config.zeta_max,
            in_flight: Mutex::new([0; STAGE_COUNT]),
        }
    }

    /// Current allowed push window for stage i.
    pub fn get_window_size(&self, stage: StageId) -> u32 {
        self.zeta[stage.index()].load(Ordering::Relaxed)
    }

    /// Backpressure weight for a stage.
    pub fn get_backpressure_weight(&self, stage: StageId) -> u64 {
        self.w[stage.index()].load(Ordering::Relaxed)
    }

    /// Number of batches currently in a stage.
    pub fn in_flight_count(&self, stage: StageId) -> u32 {
        self.in_flight.lock()[stage.index()]
    }

    /// Whether stage can accept another batch (window not full and not zero).
    pub fn can_push(&self, stage: StageId) -> bool {
        let window = self.get_window_size(stage);
        if window == 0 {
            return false;
        }
        self.in_flight_count(stage) < window
    }

    /// Batch enters a stage — update backpressure weight.
    pub fn on_batch_enter(&self, stage: StageId, gas_or_bytes: u64) {
        self.w[stage.index()].fetch_add(gas_or_bytes, Ordering::Relaxed);
        self.in_flight.lock()[stage.index()] += 1;
    }

    /// Batch leaves a stage — update weight and trigger AIMD tick.
    pub fn on_batch_exit(&self, stage: StageId, gas_or_bytes: u64) {
        self.w[stage.index()].fetch_sub(gas_or_bytes, Ordering::Relaxed);
        {
            let mut in_flight = self.in_flight.lock();
            in_flight[stage.index()] = in_flight[stage.index()].saturating_sub(1);
        }
        self.tick(stage);
    }

    /// AIMD window control — called when a batch exits stage i.
    fn tick(&self, stage: StageId) {
        let next_stage = match stage.next() {
            Some(s) => s,
            None => return,
        };

        let w_next = self.get_backpressure_weight(next_stage);
        let lambda = self.lambda[next_stage.index()].load(Ordering::Relaxed);
        let idx = stage.index();

        let current = self.zeta[idx].load(Ordering::Relaxed);
        let new_zeta = if w_next == 0 {
            let doubled = current.saturating_mul(2);
            let capped = doubled.min(self.zeta_max);
            if capped == 0 { 1 } else { capped }
        } else if w_next < lambda {
            current.saturating_add(1).min(self.zeta_max)
        } else {
            0
        };

        self.zeta[idx].store(new_zeta, Ordering::Relaxed);
    }
}

impl std::fmt::Debug for PipelineController {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("PipelineController")
            .field("zeta_order", &self.get_window_size(StageId::Order))
            .field("zeta_exec", &self.get_window_size(StageId::Exec))
            .field("zeta_commit", &self.get_window_size(StageId::Commit))
            .field("w_exec", &self.get_backpressure_weight(StageId::Exec))
            .field("w_commit", &self.get_backpressure_weight(StageId::Commit))
            .finish()
    }
}
