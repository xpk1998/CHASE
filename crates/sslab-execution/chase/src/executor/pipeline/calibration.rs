use super::metrics::WorkloadSample;

/// Recommended AIMD overload thresholds derived from observed workload.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct LambdaRecommendation {
    pub lambda2: u64,
    pub lambda3: u64,
    pub p95_gas_per_batch: u64,
    pub p95_delta_bytes_per_batch: u64,
    pub sample_count: usize,
}

impl LambdaRecommendation {
    pub fn format_report(&self) -> String {
        format!(
            "lambda calibration: lambda2={} (p95_gas={}), lambda3={} (p95_delta={} bytes), samples={}",
            self.lambda2,
            self.p95_gas_per_batch,
            self.lambda3,
            self.p95_delta_bytes_per_batch,
            self.sample_count,
        )
    }
}

/// Recommend `lambda2`/`lambda3` from workload samples.
///
/// Uses p95 per-batch gas/delta scaled by `zeta_max` and a safety factor of 2,
/// matching AIMD "light load" headroom before multiplicative decrease.
pub fn recommend_lambdas(samples: &[WorkloadSample], zeta_max: u32) -> LambdaRecommendation {
    let gas: Vec<u64> = samples.iter().map(|s| s.gas_weight).collect();
    let deltas: Vec<u64> = samples.iter().map(|s| s.delta_bytes).collect();

    let p95_gas = percentile_u64(&gas, 95).unwrap_or(1);
    let p95_delta = percentile_u64(&deltas, 95).unwrap_or(1);
    let z = zeta_max.max(1) as u64;

    LambdaRecommendation {
        lambda2: p95_gas.saturating_mul(z).saturating_mul(2),
        lambda3: p95_delta.saturating_mul(z).saturating_mul(2),
        p95_gas_per_batch: p95_gas,
        p95_delta_bytes_per_batch: p95_delta,
        sample_count: samples.len(),
    }
}

fn percentile_u64(values: &[u64], pct: u8) -> Option<u64> {
    if values.is_empty() {
        return None;
    }
    let mut sorted = values.to_vec();
    sorted.sort_unstable();
    let idx = ((pct as f64 / 100.0) * (sorted.len() - 1) as f64).round() as usize;
    Some(sorted[idx.min(sorted.len() - 1)])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn recommend_lambdas_scales_by_zeta_max() {
        let samples: Vec<WorkloadSample> = (0..100)
            .map(|i| WorkloadSample {
                gas_weight: 10_000 + i,
                delta_bytes: 4096 + i,
            })
            .collect();
        let rec = recommend_lambdas(&samples, 4);
        assert!(rec.lambda2 > rec.p95_gas_per_batch);
        assert!(rec.lambda3 > rec.p95_delta_bytes_per_batch);
        assert_eq!(rec.sample_count, 100);
    }

    #[test]
    fn percentile_empty_returns_none() {
        assert!(percentile_u64(&[], 95).is_none());
    }
}
