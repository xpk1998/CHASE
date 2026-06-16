use ethers_core::types::H256;
use itertools::Itertools;
use narwhal_types::BatchDigest;
use rayon::prelude::*;
use sslab_execution::{
    evm_storage::{
        backend::{ApplyBackend, CMemoryBackend, ExecutionBackend},
        EvmStorage,
    },
    executor::Executable,
    types::{ExecutableEthereumBatch, ExecutionResult, IndexedEthereumTransaction},
};
use evm::backend::Backend;

type DefaultBackend = CMemoryBackend;
use std::sync::Arc;
use parking_lot::Mutex;
use tracing::warn;

use crate::{
    address_based_conflict_graph::FastHashMap,
    seer::{seer_simulate_batch, SeerContext},
    types::{
        is_disjoint, AbortedTransaction, FinalizedTransaction, ReExecutedTransaction,
    },
    AddressBasedConflictGraph, SimulationResult,
};

use super::{address_based_conflict_graph::Transaction, types::SimulatedTransaction};

#[async_trait::async_trait]
impl<B: Backend + ApplyBackend + Clone + Default + Send + Sync + 'static> Executable for Chase<B> {
    async fn execute(&self, consensus_output: Vec<ExecutableEthereumBatch>) {
        let _ = self.inner.prepare_execution(consensus_output).await;
    }
}

pub struct Chase<B = DefaultBackend>
where
    B: Backend + ApplyBackend + Clone + Default + Send + Sync + 'static,
{
    inner: ConcurrencyLevelManager<B>,
}

impl<B: Backend + ApplyBackend + Clone + Default + Send + Sync + 'static> Chase<B> {
    pub fn new(global_state: EvmStorage<B>, concurrency_level: usize) -> Self {
        Self {
            inner: ConcurrencyLevelManager::new(global_state, concurrency_level),
        }
    }

    pub fn manager(&self) -> &ConcurrencyLevelManager<B> {
        &self.inner
    }

    pub fn ev_blp_bridge(
        &self,
        store: Option<Arc<dyn crate::executor::cache::StateStore>>,
    ) -> crate::executor::EvBlpChaseBridge<B> {
        crate::executor::EvBlpChaseBridge::new(self.inner.clone(), store)
    }
}

#[derive(Clone)]
pub struct ConcurrencyLevelManager<B = DefaultBackend>
where
    B: Backend + ApplyBackend + Clone + Default + Send + Sync + 'static,
{
    concurrency_level: usize,
    global_state: Arc<EvmStorage<B>>,
    seer_ctx: Arc<SeerContext>,
    /// When set, all applied effects during execution are recorded here.
    effect_collector: Arc<Mutex<Option<Vec<evm::backend::Apply>>>>,
}

impl<B: Backend + ApplyBackend + Clone + Default + Send + Sync + 'static> ConcurrencyLevelManager<B> {
    pub fn new(global_state: EvmStorage<B>, concurrency_level: usize) -> Self {
        Self::with_seer(global_state, concurrency_level, SeerContext::from_env())
    }

    pub fn with_seer(
        global_state: EvmStorage<B>,
        concurrency_level: usize,
        seer_ctx: SeerContext,
    ) -> Self {
        Self {
            global_state: Arc::new(global_state),
            concurrency_level,
            seer_ctx: Arc::new(seer_ctx),
            effect_collector: Arc::new(Mutex::new(None)),
        }
    }

    /// Execute a single batch and return digests plus all applied state effects.
    pub async fn execute_batch_with_effects(
        &self,
        batch: ExecutableEthereumBatch,
    ) -> (Vec<BatchDigest>, Vec<evm::backend::Apply>) {
        *self.effect_collector.lock() = Some(Vec::new());
        let digests = self._execute(vec![batch]).await;
        let effects = self.effect_collector.lock().take().unwrap_or_default();
        (digests, effects)
    }

    pub fn global_state(&self) -> Arc<EvmStorage<B>> {
        self.global_state.clone()
    }

    pub fn seer_context(&self) -> Arc<SeerContext> {
        self.seer_ctx.clone()
    }

    async fn prepare_execution(
        &self,
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> ExecutionResult {
        let mut result = vec![];
        let mut target = consensus_output;

        while !target.is_empty() {
            let split_idx = std::cmp::min(self.concurrency_level, target.len());
            let remains: Vec<ExecutableEthereumBatch> = target.split_off(split_idx);

            result.extend(self._execute(target).await);

            target = remains;
        }

        ExecutionResult::new(result)
    }

    async fn _unpack_batches(
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> (Vec<BatchDigest>, Vec<IndexedEthereumTransaction>) {
        let (send, recv) = tokio::sync::oneshot::channel();

        rayon::spawn(move || {
            let (digests, batches): (Vec<_>, Vec<_>) = consensus_output
                .par_iter()
                .map(|batch| (batch.digest().to_owned(), batch.data().to_owned()))
                .unzip();

            let tx_list = batches
                .into_iter()
                .flatten()
                .enumerate()
                .map(|(id, tx)| IndexedEthereumTransaction::new(tx, id as u64))
                .collect::<Vec<_>>();

            let _ = send.send((digests, tx_list)).unwrap();
        });

        recv.await.unwrap()
    }

    pub async fn _execute(
        &self,
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> Vec<BatchDigest> {
        let (digests, tx_list) = Self::_unpack_batches(consensus_output).await;

        let scheduled_aborted_txs: Vec<Vec<AbortedTransaction>>;

        // 1st execution
        {
            let rw_sets = self._simulate(tx_list).await;

            let schedule = AddressBasedConflictGraph::par_construct(rw_sets)
                .await
                .hierarchcial_sort()
                .reorder()
                .par_extract_schedule()
                .await;

            scheduled_aborted_txs = self._commit_cds_schedule(schedule).await;
        }

        for tx_list_to_re_execute in scheduled_aborted_txs.into_iter() {
            // 2nd execution
            //  (1) re-simulation  ----------------> (rw-sets are changed ??)  -------yes-------> (2') invalidate (or, fallback)
            //                                                 |
            //                                                no
            //                                                 |
            //                                          (2) commit

            let rw_sets = self
                ._re_execute(
                    tx_list_to_re_execute
                        .into_iter()
                        .map(|tx| tx.into_raw_tx())
                        .collect(),
                )
                .await;

            match self._validate_optimistic_assumption(rw_sets).await {
                None => {}
                Some(invalid_txs) => {
                    //* invalidate */
                    tracing::debug!("invalidated txs: {:?}", invalid_txs);

                    //* fallback */
                    // let ScheduledInfo {scheduled_txs, aborted_txs } = AddressBasedConflictGraph::par_construct(rw_sets).await
                    //     .hierarchcial_sort()
                    //     .reorder()
                    //     .par_extract_schedule().await;

                    // self._concurrent_commit(scheduled_txs).await;

                    //* 3rd execution (serial) for complex transactions */
                    // let snapshot = self.global_state.clone();
                    // tokio::task::spawn_blocking(move || {
                    //     aborted_txs.into_iter()
                    //         .flatten()
                    //         .for_each(|tx| {
                    //             match evm_utils::simulate_tx(tx.raw_tx(), snapshot.as_ref()) {
                    //                 Ok(Some((effect, _, _))) => {
                    //                     snapshot.apply_local_effect(effect);
                    //                 },
                    //                 _ => {
                    //                     warn!("fail to execute a transaction {}", tx.id());
                    //                 }
                    //             }
                    //         });
                    // }).await.expect("fail to spawn a task for serial execution of aborted txs");
                }
            }
        }

        digests
    }

    pub async fn simulate(
        &self,
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> SimulationResult {
        let (digests, tx_list) = Self::_unpack_batches(consensus_output).await;
        let rw_sets = self._simulate(tx_list).await;

        SimulationResult { digests, rw_sets }
    }

    async fn _simulate(
        &self,
        tx_list: Vec<IndexedEthereumTransaction>,
    ) -> Vec<SimulatedTransaction> {
        let snapshot = self.global_state.clone();
        let seer_ctx = self.seer_ctx.clone();

        // Parallel simulation requires heavy cpu usages.
        // CPU-bound jobs would make the I/O-bound tokio threads starve.
        // To this end, a separated thread pool need to be used for cpu-bound jobs.
        // a new thread is created, and a new thread pool is created on the thread. (specifically, rayon's thread pool is created)
        let (send, recv) = tokio::sync::oneshot::channel();
        rayon::spawn(move || {
            let result = if seer_ctx.config.is_enabled() {
                if seer_ctx.config.contract_locality_ordering {
                    // Warm cache / VarTable with locality ordering, then parallelize.
                    let ordered = crate::seer::order_for_contract_locality(tx_list);
                    ordered
                        .into_par_iter()
                        .filter_map(|tx| {
                            match crate::seer::seer_simulate_tx(tx.data(), snapshot.as_ref(), &seer_ctx) {
                                Ok(Some((effect, log, rw_set))) => {
                                    Some(SimulatedTransaction::new(rw_set, effect, log, tx))
                                }
                                _ => {
                                    warn!("fail to execute a transaction {}", tx.digest_u64());
                                    None
                                }
                            }
                        })
                        .collect()
                } else {
                    seer_simulate_batch(tx_list, snapshot.as_ref(), &seer_ctx)
                }
            } else {
                tx_list
                    .into_par_iter()
                    .filter_map(|tx| {
                        match crate::evm_utils::simulate_tx(tx.data(), snapshot.as_ref()) {
                            Ok(Some((effect, log, rw_set))) => {
                                Some(SimulatedTransaction::new(rw_set, effect, log, tx))
                            }
                            _ => {
                                warn!("fail to execute a transaction {}", tx.digest_u64());
                                None
                            }
                        }
                    })
                    .collect()
            };

            let _ = send.send(result).unwrap();
        });

        match recv.await {
            Ok(rw_sets) => rw_sets,
            Err(e) => {
                panic!(
                    "fail to receive simulation result from the worker thread. {:?}",
                    e
                );
            }
        }
    }

    async fn _re_execute(
        &self,
        tx_list: Vec<IndexedEthereumTransaction>,
    ) -> Vec<ReExecutedTransaction> {
        let snapshot = self.global_state.clone();
        let seer_ctx = self.seer_ctx.clone();

        // Parallel simulation requires heavy cpu usages.
        // CPU-bound jobs would make the I/O-bound tokio threads starve.
        // To this end, a separated thread pool need to be used for cpu-bound jobs.
        // a new thread is created, and a new thread pool is created on the thread. (specifically, rayon's thread pool is created)
        let (send, recv) = tokio::sync::oneshot::channel();
        rayon::spawn(move || {
            let simulate = |tx: &IndexedEthereumTransaction| {
                if seer_ctx.config.is_enabled() {
                    crate::seer::seer_simulate_tx(tx.data(), snapshot.as_ref(), &seer_ctx)
                } else {
                    crate::evm_utils::simulate_tx(tx.data(), snapshot.as_ref())
                }
            };

            let result = tx_list
                .into_par_iter()
                .filter_map(|tx| {
                    match simulate(&tx) {
                        Ok(Some((effect, log, rw_set))) => {
                            Some(ReExecutedTransaction::build_from(tx, effect, log, rw_set))
                        }
                        _ => {
                            warn!("fail to execute a transaction {}", tx.digest_u64());
                            None
                        }
                    }
                })
                .collect();

            let _ = send.send(result).unwrap();
        });

        match recv.await {
            Ok(rw_sets) => rw_sets,
            Err(e) => {
                panic!(
                    "fail to receive simulation result from the worker thread. {:?}",
                    e
                );
            }
        }
    }

    /// Commit CDS conflict-free zone: parallel chains, serial within each chain.
    #[cfg(not(feature = "latency"))]
    pub async fn _commit_cds_conflict_free_zone(
        &self,
        chains: Vec<Vec<FinalizedTransaction>>,
    ) {
        let storage = self.global_state.clone();
        let collector = self.effect_collector.clone();
        let (send, recv) = tokio::sync::oneshot::channel();
        rayon::spawn(move || {
            chains.into_par_iter().for_each(|chain| {
                for tx in chain {
                    let effect = tx.extract();
                    if let Some(sink) = collector.lock().as_mut() {
                        sink.extend(effect.clone());
                    }
                    storage.apply_local_effect(effect);
                }
            });
            let _ = send.send(());
        });
        let _ = recv.await;
    }

    #[cfg(feature = "latency")]
    pub async fn _commit_cds_conflict_free_zone(
        &self,
        chains: Vec<Vec<FinalizedTransaction>>,
    ) -> u128 {
        let storage = self.global_state.clone();
        let (send, recv) = tokio::sync::oneshot::channel();
        rayon::spawn(move || {
            let start = std::time::Instant::now();
            chains.into_par_iter().for_each(|chain| {
                for tx in chain {
                    storage.apply_local_effect(tx.extract());
                }
            });
            let _ = send.send(start.elapsed().as_micros());
        });
        recv.await.unwrap_or(0)
    }

    /// Commit CDS first pass: conflict-free chains (serial within chain, parallel across chains)
    /// followed by conflict-zone epochs (parallel within epoch).
    #[cfg(not(feature = "latency"))]
    pub async fn _commit_cds_schedule(
        &self,
        schedule: ScheduledInfo,
    ) -> Vec<Vec<AbortedTransaction>> {
        self._commit_cds_conflict_free_zone(schedule.conflict_free_zone)
            .await;
        self._concurrent_commit(schedule.conflict_zone_finalized)
            .await;
        schedule.conflict_zone_aborted
    }

    #[cfg(feature = "latency")]
    pub async fn _commit_cds_schedule(
        &self,
        schedule: ScheduledInfo,
    ) -> (Vec<Vec<AbortedTransaction>>, u128) {
        let cf_latency = self
            ._commit_cds_conflict_free_zone(schedule.conflict_free_zone)
            .await;
        let cz_latency = self
            ._concurrent_commit(schedule.conflict_zone_finalized)
            .await;
        (schedule.conflict_zone_aborted, cf_latency + cz_latency)
    }

    //TODO: (optimization) commit the last write of each key
    #[cfg(not(feature = "latency"))]
    pub async fn _concurrent_commit(&self, scheduled_txs: Vec<Vec<FinalizedTransaction>>) {
        let storage = self.global_state.clone();
        let collector = self.effect_collector.clone();

        let (send, recv) = tokio::sync::oneshot::channel();
        rayon::spawn(move || {
            let _storage = &storage;
            for txs_to_commit in scheduled_txs {
                txs_to_commit.into_par_iter().for_each(|tx| {
                    let effect = tx.extract();
                    if let Some(sink) = collector.lock().as_mut() {
                        sink.extend(effect.clone());
                    }
                    _storage.apply_local_effect(effect)
                })
            }
            let _ = send.send(());
        });

        let _ = recv.await;
    }

    #[cfg(feature = "latency")]
    pub async fn _concurrent_commit(&self, scheduled_txs: Vec<Vec<FinalizedTransaction>>) -> u128 {
        let storage = self.global_state.clone();

        // Parallel simulation requires heavy cpu usages.
        // CPU-bound jobs would make the I/O-bound tokio threads starve.
        // To this end, a separated thread pool need to be used for cpu-bound jobs.
        // a new thread is created, and a new thread pool is created on the thread. (specifically, rayon's thread pool is created)
        let (send, recv) = tokio::sync::oneshot::channel();
        rayon::spawn(move || {
            let _storage = &storage;

            let mut latency = 0u128;
            let clock = std::time::Instant::now();
            for txs_to_commit in scheduled_txs {
                let tx_len = txs_to_commit.len() as u128;
                txs_to_commit.into_par_iter().for_each(|tx| {
                    let effect = tx.extract();
                    _storage.apply_local_effect(effect)
                });
                latency += tx_len * clock.elapsed().as_micros();
            }
            let _ = send.send(latency);
        });

        recv.await.unwrap()
    }

    async fn _validate_optimistic_assumption(
        &self,
        rw_set: Vec<ReExecutedTransaction>,
    ) -> Option<Vec<ReExecutedTransaction>> {
        if rw_set.len() == 1 {
            self._concurrent_commit_2(rw_set).await;
            return None;
        }

        let (send, recv) = tokio::sync::oneshot::channel();
        rayon::spawn(move || {
            let mut valid_txs = vec![];
            let mut invalid_txs = vec![];

            let mut write_set = hashbrown::HashSet::<H256>::new();
            for tx in rw_set.into_iter() {
                let set = tx.write_set();

                if is_disjoint(&set, &write_set) {
                    write_set.extend(set);
                    valid_txs.push(tx);
                } else {
                    invalid_txs.push(tx);
                }
            }

            if invalid_txs.is_empty() {
                let _ = send.send((valid_txs, None));
            } else {
                let _ = send.send((valid_txs, Some(invalid_txs)));
            }
        });

        let (valid_txs, invalid_txs) = recv.await.unwrap();

        self._concurrent_commit_2(valid_txs).await;

        invalid_txs
    }

    pub async fn _concurrent_commit_2(&self, scheduled_txs: Vec<ReExecutedTransaction>) {
        let scheduled_txs = vec![scheduled_txs //TODO: compare to into_par_iter()
            .into_iter()
            .map(FinalizedTransaction::from)
            .collect_vec()];

        self._concurrent_commit(scheduled_txs).await;
    }
}
#[cfg(feature = "latency")]
use tokio::time::Instant;

#[cfg(feature = "latency")]
#[async_trait::async_trait]
pub trait LatencyBenchmark {
    async fn _execute_and_return_latency(
        &self,
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> (u128, u128, u128, u128, u128, u128, f64);

    async fn _validate_optimistic_assumption_and_return_latency(
        &self,
        rw_set: Vec<ReExecutedTransaction>,
    ) -> (Option<Vec<ReExecutedTransaction>>, u128, u128);
}

#[cfg(feature = "latency")]
#[async_trait::async_trait]
impl<B: Backend + ApplyBackend + Clone + Default + Send + Sync + 'static> LatencyBenchmark
    for ConcurrencyLevelManager<B>
{
    async fn _execute_and_return_latency(
        &self,
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> (u128, u128, u128, u128, u128, u128, f64) {
        let (_, tx_list) = Self::_unpack_batches(consensus_output).await;
        let total_tx_len = tx_list.len();

        let scheduled_aborted_txs: Vec<Vec<AbortedTransaction>>;

        let mut simulation_latency = 0;
        let mut scheduling_latency = 0;
        let mut v_val_latency = 0;
        let mut v_exec_latency = 0;
        let mut commit_latency = 0;

        let total_latency = Instant::now();
        let mut tx_latency = 0u128;
        // 1st execution
        {
            let latency = Instant::now();
            let rw_sets = self._simulate(tx_list).await;
            simulation_latency += latency.elapsed().as_micros();

            let latency = Instant::now();
            let schedule = AddressBasedConflictGraph::par_construct(rw_sets)
                .await
                .hierarchcial_sort()
                .reorder()
                .par_extract_schedule()
                .await;
            scheduling_latency += latency.elapsed().as_micros();

            let tx_len = schedule.scheduled_txs_len() as u128;
            let latency = Instant::now();
            let (aborted, commit_part) = self._commit_cds_schedule(schedule).await;
            tx_latency += total_latency.elapsed().as_micros() * tx_len + commit_part;
            commit_latency += latency.elapsed().as_micros();

            scheduled_aborted_txs = aborted;
        }

        for tx_list_to_re_execute in scheduled_aborted_txs.into_iter() {
            // 2nd execution
            //  (1) re-simulation  ----------------> (rw-sets are changed ??)  -------yes-------> (2') invalidate (or, fallback)
            //                                                 |
            //                                                no
            //                                                 |
            //                                          (2) commit
            let txss: Vec<IndexedEthereumTransaction> = tx_list_to_re_execute
                .into_par_iter()
                .map(|tx| tx.into_raw_tx())
                .collect();
            let tx_len = txss.len() as u128;

            let latency = Instant::now();
            let rw_sets = self._re_execute(txss).await;
            v_exec_latency += latency.elapsed().as_micros();

            match self
                ._validate_optimistic_assumption_and_return_latency(rw_sets)
                .await
            {
                (None, v, c) => {
                    commit_latency += c;
                    v_val_latency += v;
                }
                (Some(invalid_txs), v, c) => {
                    commit_latency += c;
                    v_val_latency += v;

                    //* invalidate */
                    tracing::debug!("invalidated txs: {:?}", invalid_txs);
                }
            }

            tx_latency += total_latency.elapsed().as_micros() * tx_len;
        }

        (
            total_latency.elapsed().as_micros(),
            simulation_latency,
            scheduling_latency,
            v_exec_latency,
            v_val_latency,
            commit_latency,
            tx_latency as f64 / total_tx_len as f64,
        )
    }

    async fn _validate_optimistic_assumption_and_return_latency(
        &self,
        rw_set: Vec<ReExecutedTransaction>,
    ) -> (Option<Vec<ReExecutedTransaction>>, u128, u128) {
        if rw_set.len() == 1 {
            let latency = Instant::now();
            self._concurrent_commit_2(rw_set).await;

            return (None, 0, latency.elapsed().as_micros());
        }

        let (send, recv) = tokio::sync::oneshot::channel();

        let latency = Instant::now();
        rayon::spawn(move || {
            let mut valid_txs = vec![];
            let mut invalid_txs = vec![];

            let mut write_set = hashbrown::HashSet::<H256>::new();
            for tx in rw_set.into_iter() {
                let set = tx.write_set();

                if is_disjoint(&set, &write_set) {
                    write_set.extend(set);
                    valid_txs.push(tx);
                } else {
                    invalid_txs.push(tx);
                }
            }

            if invalid_txs.is_empty() {
                let _ = send.send((valid_txs, None));
            } else {
                let _ = send.send((valid_txs, Some(invalid_txs)));
            }
        });

        let (valid_txs, invalid_txs) = recv.await.unwrap();
        let validation_latency = latency.elapsed().as_micros();

        let commit_latency = Instant::now();
        self._concurrent_commit_2(valid_txs).await;

        (
            invalid_txs,
            validation_latency,
            commit_latency.elapsed().as_micros(),
        )
    }
}

#[cfg(all(feature = "parallelism-analysis", feature = "disable-early-detection"))]
#[async_trait::async_trait]
pub trait Benchmark {
    /// when the 'last-committer-wins' feature is activated, this function measures the parallelism of LCW,
    /// otherwise, first-committer-wins rule is applied.
    async fn _analysis_parallelism_of_vanilla(
        &self,
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> (f64, f64, f64, f64, f64, u32);

    async fn _analysis_parallelism_of_chase(
        &self,
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> (f64, f64, f64, f64, f64, u32);
}
#[cfg(all(feature = "parallelism-analysis", feature = "disable-early-detection"))]
use crate::address_based_conflict_graph::Benchmark as _;
#[cfg(all(feature = "parallelism-analysis", feature = "disable-early-detection"))]
use incr_stats::incr::Stats;

#[cfg(all(feature = "parallelism-analysis", feature = "disable-early-detection"))]
#[async_trait::async_trait]
impl<B: Backend + ApplyBackend + Clone + Default + Send + Sync + 'static> Benchmark
    for ConcurrencyLevelManager<B>
{
    async fn _analysis_parallelism_of_vanilla(
        &self,
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> (f64, f64, f64, f64, f64, u32) {
        let (_, tx_list) = Self::_unpack_batches(consensus_output).await;
        let rw_sets = self._simulate(tx_list).await;

        let schedule = AddressBasedConflictGraph::construct_without_early_detection(rw_sets)
            .hierarchcial_sort()
            .reorder()
            .par_extract_schedule()
            .await;

        let mut stat = Stats::new();
        schedule
            .conflict_free_zone
            .iter()
            .chain(schedule.conflict_zone_finalized.iter())
            .for_each(|seq| {
                stat.update(seq.len() as f64).ok();
            });

        let metric = (
            stat.sum().unwrap_or_default(),
            stat.mean().unwrap_or_default(),
            stat.population_standard_deviation().unwrap_or_default(),
            stat.population_skewness().unwrap_or_default(),
            stat.max().unwrap_or_default(),
            stat.count(),
        );

        metric
    }

    async fn _analysis_parallelism_of_chase(
        &self,
        consensus_output: Vec<ExecutableEthereumBatch>,
    ) -> (f64, f64, f64, f64, f64, u32) {
        let (_, tx_list) = Self::_unpack_batches(consensus_output).await;
        let rw_sets = self._simulate(tx_list).await;

        let schedule = AddressBasedConflictGraph::par_construct(rw_sets)
            .await
            .hierarchcial_sort()
            .reorder()
            .par_extract_schedule()
            .await;

        let mut stat = Stats::new();
        schedule
            .conflict_free_zone
            .iter()
            .chain(schedule.conflict_zone_finalized.iter())
            .for_each(|seq| {
                stat.update(seq.len() as f64).ok();
            });

        schedule.conflict_zone_aborted.iter().for_each(|seq| {
            stat.update(seq.len() as f64).ok();
        });

        let metric = (
            stat.sum().unwrap_or_default(),
            stat.mean().unwrap_or_default(),
            stat.population_standard_deviation().unwrap_or_default(),
            stat.population_skewness().unwrap_or_default(),
            stat.max().unwrap_or_default(),
            stat.count(),
        );

        metric
    }
}

/// CDS scheduling result.
///
/// - `conflict_free_zone`: CHASE commit wave 1 → parallel dependency chains (CDS 无冲突区).
/// - `conflict_zone_finalized`: CHASE commit waves 2+ → inter-epoch reordering (CDS 冲突区).
/// - `conflict_zone_aborted`: aborted txs → inter-epoch reordering (CDS 冲突区, re-execution).
pub struct ScheduledInfo {
    pub conflict_free_zone: Vec<Vec<FinalizedTransaction>>,
    pub conflict_zone_finalized: Vec<Vec<FinalizedTransaction>>,
    pub conflict_zone_aborted: Vec<Vec<AbortedTransaction>>,
}

impl ScheduledInfo {
    pub fn from(
        tx_list: FastHashMap<u64, Arc<Transaction>>,
        aborted_txs: Vec<Arc<Transaction>>,
    ) -> Self {
        Self::build_cds_schedule(tx_list, aborted_txs, false)
    }

    pub fn par_from(
        tx_list: FastHashMap<u64, Arc<Transaction>>,
        aborted_txs: Vec<Arc<Transaction>>,
    ) -> Self {
        Self::build_cds_schedule(tx_list, aborted_txs, true)
    }

    fn build_cds_schedule(
        tx_list: FastHashMap<u64, Arc<Transaction>>,
        aborted_txs: Vec<Arc<Transaction>>,
        rayon: bool,
    ) -> Self {
        if rayon {
            tx_list.par_iter().for_each(|(_, tx)| tx.clear_write_units());
        } else {
            tx_list.iter().for_each(|(_, tx)| tx.clear_write_units());
        }

        let mut groups = crate::cds_scheduler::group_by_sequence(tx_list, rayon);

        let first_epoch_txs = if groups.is_empty() {
            Vec::new()
        } else {
            groups.remove(0)
        };
        let seq2plus_txs: Vec<Arc<Transaction>> = groups.into_iter().flatten().collect();

        let conflict_free_zone =
            crate::cds_scheduler::extract_conflict_free_chains(first_epoch_txs, rayon);
        let conflict_zone_finalized =
            crate::cds_scheduler::schedule_conflict_zone_finalized(seq2plus_txs, rayon);
        let conflict_zone_aborted =
            crate::cds_scheduler::schedule_conflict_zone_aborted(aborted_txs, rayon);

        Self {
            conflict_free_zone,
            conflict_zone_finalized,
            conflict_zone_aborted,
        }
    }

    /// First-pass finalized epochs in CDS commit order.
    pub fn first_pass_epochs(&self) -> Vec<Vec<FinalizedTransaction>> {
        let mut epochs = self.conflict_free_zone.clone();
        epochs.extend(self.conflict_zone_finalized.clone());
        epochs
    }

    /// Flattened first-pass schedule for metrics only.
    ///
    /// Do not pass this to [`ConcurrencyLevelManager::_concurrent_commit`]: conflict-free
    /// chains require serial execution within each chain. Use
    /// [`ConcurrencyLevelManager::_commit_cds_schedule`] instead.
    pub fn scheduled_txs(&self) -> Vec<Vec<FinalizedTransaction>> {
        self.first_pass_epochs()
    }

    pub fn aborted_txs(&self) -> Vec<Vec<AbortedTransaction>> {
        self.conflict_zone_aborted.clone()
    }

    pub fn scheduled_txs_len(&self) -> usize {
        self.conflict_free_zone
            .iter()
            .chain(self.conflict_zone_finalized.iter())
            .map(|vec| vec.len())
            .sum()
    }

    pub fn aborted_txs_len(&self) -> usize {
        self.conflict_zone_aborted.iter().map(|vec| vec.len()).sum()
    }

    pub fn parallism_metric(&self) -> (usize, f64, f64, usize, usize) {
        let epochs = self.first_pass_epochs();
        let total_tx = self.scheduled_txs_len() + self.aborted_txs_len();
        let max_width = epochs.iter().map(|vec| vec.len()).max().unwrap_or(0);
        let depth = epochs.len().max(1);
        let average_width =
            epochs.iter().map(|vec| vec.len()).sum::<usize>() as f64 / depth as f64;
        let var_width = epochs
            .iter()
            .map(|vec| vec.len())
            .fold(0.0, |acc, len| acc + (len as f64 - average_width).powi(2))
            / depth as f64;
        let std_width = var_width.sqrt();
        (total_tx, average_width, std_width, max_width, depth)
    }
}
