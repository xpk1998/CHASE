use fastcrypto::hash::Hash;
use narwhal_types::{BatchAPI, ConsensusOutput};
use sslab_execution::types::{EthereumTransaction, ExecutableEthereumBatch};
use tracing::warn;

/// Convert a Narwhal/Tusk [`ConsensusOutput`] into Chase-ready executable batches.
///
/// Preserves the certificate/batch ordering produced by consensus.
pub fn consensus_output_to_executable_batches(
    consensus_output: &ConsensusOutput,
) -> Vec<ExecutableEthereumBatch> {
    let mut executable_batches = Vec::new();

    for batch_group in &consensus_output.batches {
        for batch in batch_group {
            let digest = batch.digest();
            let mut transactions = Vec::with_capacity(batch.transactions().len());

            for raw_tx in batch.transactions() {
                match EthereumTransaction::from_json(raw_tx) {
                    Ok(tx) => transactions.push(tx),
                    Err(err) => {
                        warn!(
                            ?digest,
                            ?err,
                            "skipping transaction that failed JSON deserialization"
                        );
                    }
                }
            }

            executable_batches.push(ExecutableEthereumBatch::new(transactions, digest));
        }
    }

    executable_batches
}

#[cfg(test)]
mod tests {
    use super::*;
    use narwhal_test_utils::latest_protocol_version;
    use narwhal_types::{Batch, CommittedSubDag, ConsensusOutput};
    use std::sync::Arc;

    #[test]
    fn converts_consensus_output_batches() {
        let tx = serde_json::to_vec(&ethers_core::types::transaction::eip2718::TypedTransaction::Legacy(
            ethers_core::types::TransactionRequest {
                from: Some(ethers_core::types::Address::zero()),
                to: Some(ethers_core::types::NameOrAddress::Address(
                    ethers_core::types::Address::zero(),
                )),
                value: Some(ethers_core::types::U256::zero()),
                gas: Some(ethers_core::types::U256::from(21_000)),
                ..Default::default()
            }
            .into(),
        ))
        .unwrap();

        let batch = Batch::new(vec![tx], &latest_protocol_version());
        let digest = batch.digest();

        let sub_dag = CommittedSubDag::new(
            vec![],
            Default::default(),
            1,
            Default::default(),
            None,
        );
        let output = ConsensusOutput {
            sub_dag: Arc::new(sub_dag),
            batches: vec![vec![batch]],
        };

        let exec = consensus_output_to_executable_batches(&output);
        assert_eq!(exec.len(), 1);
        assert_eq!(exec[0].digest(), &digest);
        assert_eq!(exec[0].data().len(), 1);
    }
}
