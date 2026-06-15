use narwhal_types::BatchDigest;
use sslab_execution::types::ExecutableEthereumBatch;

/// Pipeline stage identifiers.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum StageId {
    /// P₁: Order — receive consensus output, produce ordered batches.
    Order = 0,
    /// P₂: Execute — schedule and execute transactions.
    Exec = 1,
    /// P₃: Commit — persist execution results.
    Commit = 2,
}

pub const STAGE_COUNT: usize = 3;

impl StageId {
    pub fn index(self) -> usize {
        self as usize
    }

    pub fn next(self) -> Option<StageId> {
        match self {
            StageId::Order => Some(StageId::Exec),
            StageId::Exec => Some(StageId::Commit),
            StageId::Commit => None,
        }
    }

    pub fn from_index(idx: usize) -> Option<StageId> {
        match idx {
            0 => Some(StageId::Order),
            1 => Some(StageId::Exec),
            2 => Some(StageId::Commit),
            _ => None,
        }
    }
}

/// A batch flowing through the EV-BLP pipeline.
#[derive(Clone, Debug)]
pub struct PipelineBatch {
    pub batch_id: u64,
    pub batch: ExecutableEthereumBatch,
    /// Gas weight W_Bj for P₂ backpressure.
    pub gas_weight: u64,
    /// State delta bytes S_delta(Bj) for P₃ backpressure.
    pub delta_bytes: u64,
}

impl PipelineBatch {
    pub fn new(batch_id: u64, batch: ExecutableEthereumBatch) -> Self {
        let gas_weight = batch
            .data()
            .iter()
            .map(|tx| tx.gas_limit())
            .fold(0u64, |acc, g| acc.saturating_add(g));
        Self {
            batch_id,
            batch,
            gas_weight,
            delta_bytes: 0,
        }
    }

    pub fn with_delta_bytes(mut self, bytes: u64) -> Self {
        self.delta_bytes = bytes;
        self
    }

    pub fn digest(&self) -> &BatchDigest {
        self.batch.digest()
    }
}
