use criterion::{criterion_group, criterion_main, BatchSize, Criterion, Throughput};
use ethers_providers::{MockProvider, Provider};
use sslab_execution::{
    types::ExecutableEthereumBatch,
    utils::smallbank_contract_benchmark::concurrent_evm_storage,
    utils::test_utils::{SmallBankTransactionHandler, DEFAULT_CHAIN_ID},
};
use sslab_execution_chase::{
    recommend_lambdas, ConcurrencyLevelManager, EvBlpChaseBridge,
};

const DEFAULT_BATCH_SIZE: usize = 50;
const BLOCK_CONCURRENCY: usize = 4;

fn smallbank_handler() -> SmallBankTransactionHandler {
    let provider = Provider::<MockProvider>::new(MockProvider::default());
    SmallBankTransactionHandler::new(provider, DEFAULT_CHAIN_ID)
}

fn smallbank_workload(batch_size: usize, block_concurrency: usize) -> Vec<ExecutableEthereumBatch> {
    smallbank_handler().create_batches(batch_size, block_concurrency, 0.0, 100_000)
}

fn ev_blp_benchmark(c: &mut Criterion) {
    let mut group = c.benchmark_group("EV-BLP");
    group.throughput(Throughput::Elements(DEFAULT_BATCH_SIZE as u64));

    group.bench_function("chase_direct", |b| {
        b.to_async(tokio::runtime::Runtime::new().unwrap())
            .iter_batched(
                || {
                    let batches = smallbank_workload(DEFAULT_BATCH_SIZE, BLOCK_CONCURRENCY);
                    let chase = ConcurrencyLevelManager::new(concurrent_evm_storage(), BLOCK_CONCURRENCY);
                    (chase, batches)
                },
                |(chase, batches)| async move {
                    for chunk in batches.chunks(BLOCK_CONCURRENCY) {
                        chase._execute(chunk.to_vec()).await;
                    }
                },
                BatchSize::SmallInput,
            );
    });

    group.bench_function("ev_blp_pipeline", |b| {
        b.to_async(tokio::runtime::Runtime::new().unwrap())
            .iter_batched(
                || {
                    let batches = smallbank_workload(DEFAULT_BATCH_SIZE, BLOCK_CONCURRENCY);
                    let chase = ConcurrencyLevelManager::new(concurrent_evm_storage(), BLOCK_CONCURRENCY);
                    let bridge = EvBlpChaseBridge::new(chase, None);
                    (bridge, batches)
                },
                |(bridge, batches)| async move {
                    bridge.execute(batches).await;
                },
                BatchSize::SmallInput,
            );
    });

    group.finish();

    // One-shot calibration run (not timed by Criterion).
    let rt = tokio::runtime::Runtime::new().unwrap();
    rt.block_on(async {
        let batches = smallbank_workload(DEFAULT_BATCH_SIZE, BLOCK_CONCURRENCY);
        let chase = ConcurrencyLevelManager::new(concurrent_evm_storage(), BLOCK_CONCURRENCY);
        let bridge = EvBlpChaseBridge::new(chase, None);
        bridge.execute(batches).await;

        let summary = bridge.metrics().summary();
        let samples = bridge.metrics().workload_samples();
        let rec = recommend_lambdas(&samples, 8);

        println!("\n=== EV-BLP calibration (SmallBank, {} batches) ===", DEFAULT_BATCH_SIZE);
        println!("{}", summary.format_report());
        println!("{}", rec.format_report());
        println!(
            "Suggested env: CHASE_PIPELINE_LAMBDA2={} CHASE_PIPELINE_LAMBDA3={}",
            rec.lambda2, rec.lambda3
        );
    });
}

criterion_group!(benches, ev_blp_benchmark);
criterion_main!(benches);
