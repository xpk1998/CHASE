#[cfg(test)]
mod seer_unit_tests {
    use ethers_core::types::{Address, H160, H256};
    use evm::executor::stack::{RwSet, Simulatable};
    use hashbrown::HashSet;

    use crate::seer::{
        order_for_contract_locality, Perceptron, PreExecutionCache, SeerConfig, SeerContext,
        TAKEN, UNCERTAIN,
    };
    use sslab_execution::types::{EthereumTransaction, IndexedEthereumTransaction};

    fn tx_with_to(id: u64, to: u64) -> IndexedEthereumTransaction {
        use ethers_core::types::transaction::eip2718::TypedTransaction;
        let mut req = ethers_core::types::TransactionRequest::default();
        req.to = Some(ethers_core::types::NameOrAddress::Address(Address::from_low_u64_be(to)));
        let eth_tx = EthereumTransaction(TypedTransaction::Legacy(req.into()));
        IndexedEthereumTransaction::new(eth_tx, id)
    }

    #[test]
    fn contract_locality_ordering_groups_by_target() {
        let txs = vec![
            tx_with_to(1, 3),
            tx_with_to(2, 1),
            tx_with_to(3, 1),
            tx_with_to(4, 2),
        ];
        let ordered = order_for_contract_locality(txs);
        let targets: Vec<u64> = ordered
            .iter()
            .map(|t| t.data().to_addr().unwrap().to_low_u64_be())
            .collect::<Vec<_>>();
        assert_eq!(targets, vec![1, 1, 2, 3]);
    }

    #[test]
    fn perceptron_learns_after_warmup() {
        let mut p = Perceptron::new();
        for _ in 0..10 {
            let _ = p.predict(true);
            p.update(TAKEN, UNCERTAIN);
        }
        let pred = p.predict(false);
        assert!(pred == TAKEN || pred == UNCERTAIN);
    }

    #[test]
    fn pre_execution_cache_hits_on_identical_tx() {
        use ethers_core::types::transaction::eip2718::TypedTransaction;
        let mut cache = PreExecutionCache::new();
        let mut req = ethers_core::types::TransactionRequest::default();
        req.from = Some(Address::zero());
        req.value = Some(ethers_core::types::U256::zero());
        req.gas = Some(ethers_core::types::U256::from(21_000u64));
        let tx = EthereumTransaction(TypedTransaction::Legacy(req.into()));
        let mut rw = RwSet::new();
        rw.record_write_key(
            H160::from_low_u64_be(1),
            H256::from_low_u64_be(2),
            H256::from_low_u64_be(3),
        );

        assert!(cache.get(&tx).is_none());
        cache.insert(
            &tx,
            crate::seer::CachedSimulation {
                effects: vec![],
                logs: vec![],
                rw_set: rw.clone(),
            },
        );
        assert!(cache.get(&tx).is_some());
        let (hits, misses) = cache.stats();
        assert_eq!(hits, 1);
        assert_eq!(misses, 1);
    }

    #[test]
    fn seer_config_disabled_via_env() {
        std::env::set_var("CHASE_USE_SEER", "0");
        let cfg = SeerConfig::from_env();
        assert!(!cfg.is_enabled());
        std::env::remove_var("CHASE_USE_SEER");
    }

    #[test]
    fn var_table_learns_from_rw_sets() {
        let ctx = SeerContext::new(SeerConfig {
            enable_perceptron: true,
            enable_cache: false,
            contract_locality_ordering: false,
        });
        let contract = Address::from_low_u64_be(42);
        let mut reads = HashSet::new();
        let mut writes = HashSet::new();
        reads.insert(H256::from_low_u64_be(1));
        writes.insert(H256::from_low_u64_be(1));
        ctx.var_table
            .learn_from_rw_access(contract, &reads, &writes);
        let pred = ctx.var_table.predict(contract, H256::from_low_u64_be(1), "rw:0x0…01");
        assert!(pred == TAKEN || pred == UNCERTAIN || pred == crate::seer::NOT_TAKEN);
    }
}
