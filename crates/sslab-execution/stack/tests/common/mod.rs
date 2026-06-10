use std::collections::{BTreeMap, BTreeSet};
use std::str::FromStr;
use std::sync::{Arc, Mutex};

use bytes::Bytes;
use ethers_core::types::{H160, U256};
use fastcrypto::hash::Hash;
use ethers_core::utils::hex;
use ethers_providers::{MockProvider, Provider};
use evm::backend::{Apply, Basic};
use narwhal_consensus::consensus::{ConsensusState, LeaderSchedule, LeaderSwapTable};
use narwhal_consensus::metrics::ConsensusMetrics;
use narwhal_consensus::tusk::Tusk;
use narwhal_consensus::Outcome;
use narwhal_types::{Batch, CommittedSubDag, ConsensusOutput};
use prometheus::Registry;
use sslab_execution::evm_storage::backend::ApplyBackend;
use sslab_execution::utils::smallbank_contract_benchmark::{
    ADMIN_ADDRESS, CONTRACT_BYTECODE, DEFAULT_CONTRACT_ADDRESS,
};
use sslab_execution::utils::test_utils::{SmallBankTransactionHandler, DEFAULT_CHAIN_ID};
use sslab_execution_stack::ChaseStack;
use narwhal_storage::ConsensusStore;
use narwhal_test_utils::{latest_protocol_version, CommitteeFixture};
use narwhal_types::Certificate;

/// Serialize RocksDB opens across tests to avoid prometheus metric re-registration.
pub static ROCKSDB_TEST_LOCK: Mutex<()> = Mutex::new(());

/// Build JSON-encoded SmallBank transaction batches for the consensus adapter.
pub fn smallbank_json_batches(
    batch_size: usize,
    batch_num: usize,
    zipfian_coef: f32,
    account_num: u64,
) -> Vec<Vec<Bytes>> {
    let handler = smallbank_handler();
    handler
        .create_batches(batch_size, batch_num, zipfian_coef, account_num)
        .into_iter()
        .map(|batch| {
            batch
                .data()
                .iter()
                .map(|tx| Bytes::from(serde_json::to_vec(&tx.0).unwrap()))
                .collect()
        })
        .collect()
}

/// Deploy the SmallBank contract and admin account into an opened stack.
pub fn deploy_smallbank(stack: &ChaseStack) {
    let backend = stack.evm_storage.get_storage();
    let contract_addr = H160::from_str(DEFAULT_CONTRACT_ADDRESS).unwrap();
    let admin_addr = H160::from_str(ADMIN_ADDRESS).unwrap();
    let code = hex::decode(CONTRACT_BYTECODE).unwrap();

    let applies = vec![
        Apply::Modify {
            address: contract_addr,
            basic: Basic {
                nonce: U256::one(),
                balance: U256::from(10_000_000),
            },
            code: Some(code),
            storage: BTreeMap::new(),
            reset_storage: false,
        },
        Apply::Modify {
            address: admin_addr,
            basic: Basic {
                nonce: U256::one(),
                balance: U256::from(10_000_000),
            },
            code: None,
            storage: BTreeMap::new(),
            reset_storage: false,
        },
    ];
    backend.apply(applies, false);
}

pub fn smallbank_handler() -> SmallBankTransactionHandler {
    let provider = Provider::<MockProvider>::new(MockProvider::default());
    SmallBankTransactionHandler::new(provider, DEFAULT_CHAIN_ID)
}

/// Build a [`ConsensusOutput`] from raw transaction bytes for a single batch group.
pub fn build_consensus_output(
    raw_batches: Vec<Vec<Bytes>>,
    sub_dag: CommittedSubDag,
) -> ConsensusOutput {
    let protocol_config = latest_protocol_version();
    let batches = raw_batches
        .into_iter()
        .map(|txs| {
            let transactions: Vec<Vec<u8>> = txs.into_iter().map(|b| b.to_vec()).collect();
            vec![Batch::new(transactions, &protocol_config)]
        })
        .collect();

    ConsensusOutput {
        sub_dag: Arc::new(sub_dag),
        batches,
    }
}

/// Drive Tusk through one commit round (mirrors `commit_one` in bullshark tests).
pub fn tusk_commit_one() -> CommittedSubDag {
    let fixture = CommitteeFixture::builder().build();
    let committee = fixture.committee();
    let ids: Vec<_> = fixture.authorities().map(|a| a.id()).collect();
    let genesis = Certificate::genesis(&committee)
        .iter()
        .map(|x| x.digest())
        .collect::<BTreeSet<_>>();

    let (mut certificates, next_parents) = narwhal_test_utils::make_optimal_certificates(
        &committee,
        &latest_protocol_version(),
        1..=2,
        &genesis,
        &ids,
    );

    let (_, certificate) = narwhal_test_utils::mock_certificate(
        &committee,
        &latest_protocol_version(),
        ids[0],
        3,
        next_parents.clone(),
    );
    certificates.push_back(certificate);
    let (_, certificate) = narwhal_test_utils::mock_certificate(
        &committee,
        &latest_protocol_version(),
        ids[1],
        3,
        next_parents,
    );
    certificates.push_back(certificate);

    let store = Arc::new(ConsensusStore::new_for_tests());
    let metrics = Arc::new(ConsensusMetrics::new(&Registry::new()));
    let leader_schedule = LeaderSchedule::new(committee.clone(), LeaderSwapTable::default());
    let mut tusk = Tusk::new(committee, store, metrics.clone(), leader_schedule);
    let mut state = ConsensusState::new(metrics, 50);

    let mut last_committed = None;
    while let Some(certificate) = certificates.pop_front() {
        let (outcome, sub_dags) = tusk
            .process_certificate(&mut state, certificate)
            .expect("tusk should process certificate");
        if outcome == Outcome::Commit {
            assert_eq!(sub_dags.len(), 1);
            last_committed = Some(sub_dags.into_iter().next().unwrap());
        }
    }

    last_committed.expect("tusk should commit one sub-dag")
}
