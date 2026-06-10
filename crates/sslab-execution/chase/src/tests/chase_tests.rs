use std::str::FromStr;

use ethers_core::types::{H160, H256};
use evm::executor::stack::{RwSet, Simulatable};
use itertools::Itertools;
use sslab_execution::types::{EthereumTransaction, IndexedEthereumTransaction};

use crate::{
    address_based_conflict_graph::AddressBasedConflictGraph,
    types::SimulatedTransaction,
};

const CONTRACT_ADDR: u64 = 0x1;

fn transaction_with_rw(tx_id: u64, read_addr: u64, write_addr: u64) -> SimulatedTransaction {
    let mut set = RwSet::new();
    set.record_read_key(
        H160::from_low_u64_be(CONTRACT_ADDR),
        H256::from_low_u64_be(read_addr),
        H256::from_low_u64_be(1),
    );
    set.record_write_key(
        H160::from_low_u64_be(CONTRACT_ADDR),
        H256::from_low_u64_be(write_addr),
        H256::from_low_u64_be(1),
    );
    SimulatedTransaction::new(
        set,
        Vec::new(),
        Vec::new(),
        IndexedEthereumTransaction::new(EthereumTransaction::default(), tx_id),
    )
}

fn transaction_with_multiple_rw(
    tx_id: u64,
    read_addr: Vec<u64>,
    write_addr: Vec<u64>,
) -> SimulatedTransaction {
    let mut set = RwSet::new();
    read_addr.iter().for_each(|addr| {
        set.record_read_key(
            H160::from_low_u64_be(CONTRACT_ADDR),
            H256::from_low_u64_be(*addr),
            H256::from_low_u64_be(1),
        );
    });
    write_addr.iter().for_each(|addr| {
        set.record_write_key(
            H160::from_low_u64_be(CONTRACT_ADDR),
            H256::from_low_u64_be(*addr),
            H256::from_low_u64_be(1),
        );
    });
    SimulatedTransaction::new(
        set,
        Vec::new(),
        Vec::new(),
        IndexedEthereumTransaction::new(EthereumTransaction::default(), tx_id),
    )
}

fn transaction_with_multiple_rw_str(
    tx_id: u64,
    read_addr: Vec<&str>,
    write_addr: Vec<&str>,
) -> SimulatedTransaction {
    let mut set = RwSet::new();
    read_addr.into_iter().for_each(|addr| {
        set.record_read_key(
            H160::from_low_u64_be(CONTRACT_ADDR),
            H256::from_str(addr).unwrap(),
            H256::from_low_u64_be(1),
        );
    });
    write_addr.into_iter().for_each(|addr| {
        set.record_write_key(
            H160::from_low_u64_be(CONTRACT_ADDR),
            H256::from_str(addr).unwrap(),
            H256::from_low_u64_be(1),
        );
    });
    SimulatedTransaction::new(
        set,
        Vec::new(),
        Vec::new(),
        IndexedEthereumTransaction::new(EthereumTransaction::default(), tx_id),
    )
}

fn assert_cds_schedule(
    schedule: &crate::chase_core::ScheduledInfo,
    conflict_free: &[Vec<u64>],
    conflict_zone: &[Vec<u64>],
    aborted: &[Vec<u64>],
) {
    let cf: Vec<Vec<u64>> = schedule
        .conflict_free_zone
        .iter()
        .map(|chain| chain.iter().map(|tx| tx.id()).collect_vec())
        .collect();
    let cz: Vec<Vec<u64>> = schedule
        .conflict_zone_finalized
        .iter()
        .map(|epoch| epoch.iter().map(|tx| tx.id()).collect_vec())
        .collect();
    let ab: Vec<Vec<u64>> = schedule
        .conflict_zone_aborted
        .iter()
        .map(|epoch| epoch.iter().map(|tx| tx.id()).collect_vec())
        .collect();

    assert_eq!(cf, conflict_free);
    assert_eq!(cz, conflict_zone);
    assert_eq!(ab, aborted);
}

fn chase_test(
    input_txs: Vec<SimulatedTransaction>,
    conflict_free: Vec<Vec<u64>>,
    conflict_zone: Vec<Vec<u64>>,
    aborted: Vec<Vec<u64>>,
) {
    let schedule = AddressBasedConflictGraph::construct(input_txs)
        .hierarchcial_sort()
        .reorder()
        .extract_schedule();
    assert_cds_schedule(&schedule, &conflict_free, &conflict_zone, &aborted);
}

async fn chase_par_test(
    input_txs: Vec<SimulatedTransaction>,
    conflict_free: Vec<Vec<u64>>,
    conflict_zone: Vec<Vec<u64>>,
    aborted: Vec<Vec<u64>>,
) {
    let schedule = AddressBasedConflictGraph::par_construct(input_txs)
        .await
        .hierarchcial_sort()
        .reorder()
        .par_extract_schedule()
        .await;
    assert_cds_schedule(&schedule, &conflict_free, &conflict_zone, &aborted);
}

#[tokio::test]
async fn test_scenario_1() {
    let txs = vec![
        transaction_with_rw(1, 2, 1),
        transaction_with_rw(2, 3, 2),
        transaction_with_rw(3, 4, 2),
        transaction_with_rw(4, 4, 3),
        transaction_with_rw(5, 4, 4),
        transaction_with_rw(6, 1, 3),
    ];

    let conflict_free = vec![vec![2]];
    let conflict_zone = vec![vec![3, 4, 5], vec![6]];
    let aborted = vec![vec![1]];

    chase_test(
        txs.clone(),
        conflict_free.clone(),
        conflict_zone.clone(),
        aborted.clone(),
    );
    chase_par_test(txs, conflict_free, conflict_zone, aborted).await;
}

#[tokio::test]
async fn test_scenario_2() {
    let txs = vec![
        transaction_with_rw(1, 2, 1),
        transaction_with_rw(3, 4, 2),
        transaction_with_rw(2, 3, 2),
        transaction_with_rw(4, 4, 3),
        transaction_with_rw(5, 4, 4),
        transaction_with_rw(6, 1, 3),
    ];

    let conflict_free = vec![vec![3]];
    let conflict_zone = vec![vec![2, 4, 5], vec![6]];
    let aborted = vec![vec![1]];

    chase_test(
        txs.clone(),
        conflict_free.clone(),
        conflict_zone.clone(),
        aborted.clone(),
    );
    chase_par_test(txs, conflict_free, conflict_zone, aborted).await;
}

#[tokio::test]
async fn test_scenario_3() {
    let txs = vec![
        transaction_with_rw(1, 2, 1),
        transaction_with_rw(2, 3, 2),
        transaction_with_rw(3, 4, 2),
        transaction_with_rw(6, 1, 3),
        transaction_with_rw(5, 4, 4),
        transaction_with_rw(4, 4, 3),
    ];

    let conflict_free = vec![vec![2]];
    let conflict_zone = vec![vec![3, 4, 5], vec![6]];
    let aborted = vec![vec![1]];

    chase_test(
        txs.clone(),
        conflict_free.clone(),
        conflict_zone.clone(),
        aborted.clone(),
    );
    chase_par_test(txs, conflict_free, conflict_zone, aborted).await;
}

#[tokio::test]
async fn test_scenario_4() {
    let txs = vec![
        transaction_with_rw(1, 2, 1),
        transaction_with_rw(2, 3, 2),
        transaction_with_rw(3, 4, 2),
        transaction_with_rw(4, 4, 4),
        transaction_with_rw(5, 4, 4),
        transaction_with_rw(6, 1, 3),
    ];

    let conflict_free = vec![vec![1]];
    let conflict_zone = vec![vec![2, 4], vec![3]];
    let aborted = vec![vec![5, 6]];

    chase_test(
        txs.clone(),
        conflict_free.clone(),
        conflict_zone.clone(),
        aborted.clone(),
    );
    chase_par_test(txs, conflict_free, conflict_zone, aborted).await;
}

#[tokio::test]
async fn test_scenario_5() {
    let txs = vec![
        transaction_with_rw(1, 2, 1),
        transaction_with_rw(2, 3, 2),
        transaction_with_rw(3, 4, 2),
        transaction_with_rw(4, 4, 4),
        transaction_with_rw(5, 4, 4),
        transaction_with_rw(6, 1, 3),
        transaction_with_rw(7, 4, 4),
    ];

    let conflict_free = vec![vec![1]];
    let conflict_zone = vec![vec![2, 4], vec![3]];
    let aborted = vec![vec![5, 6], vec![7]];

    chase_test(
        txs.clone(),
        conflict_free.clone(),
        conflict_zone.clone(),
        aborted.clone(),
    );
    chase_par_test(txs, conflict_free, conflict_zone, aborted).await;
}

#[tokio::test]
async fn test_reordering() {
    let txs = vec![
        transaction_with_multiple_rw(1, vec![], vec![1, 2]),
        transaction_with_rw(2, 2, 1),
    ];

    let conflict_free = vec![vec![2]];
    let conflict_zone = vec![vec![1]];
    let aborted = vec![];

    chase_test(
        txs.clone(),
        conflict_free.clone(),
        conflict_zone.clone(),
        aborted.clone(),
    );
    chase_par_test(txs, conflict_free, conflict_zone, aborted).await;
}

#[tokio::test]
async fn test_scenario_6() {
    let txs = vec![
        transaction_with_multiple_rw_str(
            1,
            vec![
                "0x48c8d13a49dbf1c93484ba997be20d9cae319d82960232db3544bb8bf65d4ac0",
                "0xe3ea58be4f1efa6db4e24abc274fb1bccd82dfcd49c8f508a08c911f0357c19d",
            ],
            vec![
                "0x48c8d13a49dbf1c93484ba997be20d9cae319d82960232db3544bb8bf65d4ac0",
                "0xe3ea58be4f1efa6db4e24abc274fb1bccd82dfcd49c8f508a08c911f0357c19d",
            ],
        ),
        transaction_with_multiple_rw_str(
            2,
            vec![
                "0x7b6a909101d770fd973075a9dbcef6c7ae894d77f3f89dcacb997ab3178cd44e",
                "0xb955ea50cf68e45358af8183015c9694f0e9401fee45e367d90c462108f102bd",
            ],
            vec![
                "0x7b6a909101d770fd973075a9dbcef6c7ae894d77f3f89dcacb997ab3178cd44e",
                "0xb955ea50cf68e45358af8183015c9694f0e9401fee45e367d90c462108f102bd",
            ],
        ),
        transaction_with_multiple_rw_str(
            3,
            vec![
                "0x7b6a909101d770fd973075a9dbcef6c7ae894d77f3f89dcacb997ab3178cd44e",
                "0xe3ea58be4f1efa6db4e24abc274fb1bccd82dfcd49c8f508a08c911f0357c19d",
            ],
            vec![
                "0x7b6a909101d770fd973075a9dbcef6c7ae894d77f3f89dcacb997ab3178cd44e",
                "0xe3ea58be4f1efa6db4e24abc274fb1bccd82dfcd49c8f508a08c911f0357c19d",
            ],
        ),
    ];

    let conflict_free = vec![vec![1], vec![2]];
    let conflict_zone = vec![];
    let aborted = vec![vec![3]];

    chase_test(
        txs.clone(),
        conflict_free.clone(),
        conflict_zone.clone(),
        aborted.clone(),
    );
    chase_par_test(txs, conflict_free, conflict_zone, aborted).await;
}
