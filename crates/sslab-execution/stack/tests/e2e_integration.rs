mod common;

use std::str::FromStr;

use ethers_core::types::{H160, U256};
use ethers_core::utils::hex;
use evm::backend::{Backend, MemoryVicinity};
use narwhal_executor::ExecutionState;
use sslab_execution::utils::smallbank_contract_benchmark::{
    CONTRACT_BYTECODE, DEFAULT_CONTRACT_ADDRESS,
};
use sslab_execution_stack::{ChaseStack, PersistableCMemoryBackend};
use tempfile::tempdir;

use common::{
    build_consensus_output, deploy_smallbank, smallbank_json_batches, tusk_commit_one,
    ROCKSDB_TEST_LOCK,
};

/// ConsensusOutput → Chase → RocksDB with a SmallBank workload.
#[tokio::test]
async fn e2e_smallbank_execution_persists_to_rocksdb() {
    let _guard = ROCKSDB_TEST_LOCK.lock().unwrap_or_else(|e| e.into_inner());

    let dir = tempdir().unwrap();
    let stack = ChaseStack::open(dir.path(), 4).unwrap();
    deploy_smallbank(&stack);
    let execution_state = stack.into_execution_state();

    let raw_batches = smallbank_json_batches(5, 2, 0.0, 100);
    let sub_dag = tusk_commit_one();
    let sub_dag_index = sub_dag.sub_dag_index;
    let consensus_output = build_consensus_output(raw_batches, sub_dag);

    execution_state
        .handle_consensus_output(consensus_output)
        .await;

    assert_eq!(
        execution_state.last_executed_sub_dag_index().await,
        sub_dag_index
    );
}

/// Verify execution progress and EVM state are persisted and can be hydrated after execution.
///
/// Note: typed-store keeps a background metrics task per DBMap that holds the RocksDB
/// handle, so re-opening the same path in-process is not reliable in tests. This test
/// instead validates the same hydration path `ChaseStack::open` uses on restart.
#[tokio::test]
async fn e2e_stack_recovery_after_execution() {
    let _guard = ROCKSDB_TEST_LOCK.lock().unwrap_or_else(|e| e.into_inner());

    let dir = tempdir().unwrap();
    let stack = ChaseStack::open(dir.path(), 4).unwrap();
    deploy_smallbank(&stack);

    let raw_batches = smallbank_json_batches(4, 1, 0.0, 50);
    let sub_dag = tusk_commit_one();
    let sub_dag_index = sub_dag.sub_dag_index;
    let consensus_output = build_consensus_output(raw_batches, sub_dag);

    stack
        .execution_state
        .handle_consensus_output(consensus_output)
        .await;
    assert_eq!(
        stack.execution_state.last_executed_sub_dag_index().await,
        sub_dag_index
    );
    assert_eq!(
        stack.storage.read().last_executed_sub_dag_index().unwrap(),
        sub_dag_index
    );

    let contract_addr = H160::from_str(DEFAULT_CONTRACT_ADDRESS).unwrap();
    let recovered_backend = PersistableCMemoryBackend::new(
        MemoryVicinity {
            gas_price: U256::zero(),
            origin: H160::default(),
            chain_id: U256::one(),
            block_hashes: Vec::new(),
            block_number: Default::default(),
            block_coinbase: Default::default(),
            block_timestamp: Default::default(),
            block_difficulty: Default::default(),
            block_gas_limit: Default::default(),
            block_base_fee_per_gas: U256::zero(),
            block_randomness: None,
        },
        stack.storage.clone(),
    );
    assert!(recovered_backend.exists(contract_addr));
    assert!(!recovered_backend.code(contract_addr).is_empty());
    assert_eq!(
        recovered_backend.code(contract_addr),
        hex::decode(CONTRACT_BYTECODE).unwrap()
    );
}

/// Tusk commit → ConsensusOutput with SmallBank txs → Chase execution → RocksDB.
#[tokio::test]
async fn e2e_tusk_consensus_to_chase_execution() {
    let _guard = ROCKSDB_TEST_LOCK.lock().unwrap_or_else(|e| e.into_inner());

    let dir = tempdir().unwrap();
    let stack = ChaseStack::open(dir.path(), 4).unwrap();
    deploy_smallbank(&stack);
    let execution_state = stack.into_execution_state();

    let committed_sub_dag = tusk_commit_one();
    assert!(!committed_sub_dag.certificates.is_empty());
    assert_eq!(committed_sub_dag.leader.round(), 2);

    let raw_batches = smallbank_json_batches(3, 1, 0.0, 20);
    let consensus_output = build_consensus_output(raw_batches, committed_sub_dag.clone());

    execution_state
        .handle_consensus_output(consensus_output)
        .await;

    assert_eq!(
        execution_state.last_executed_sub_dag_index().await,
        committed_sub_dag.sub_dag_index
    );
}
