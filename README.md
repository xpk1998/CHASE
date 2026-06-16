# CHASE

**CHASE** (Conflict-aware High-performance Adaptive Scheduling Engine) is a blockchain execution system that combines deterministic concurrency control with **CDS** (Conflict Detection Scheduling) and optional **EV-BLP** (Execute-Validate Batch-Level Pipelining) for overlapping batch execution and persistence.


## Full stack

The `sslab-execution-stack` crate wires the three layers together:

```text
Transactions → Narwhal Worker → Tusk consensus → ordered batches
       → ChaseExecutionState → CDS scheduling + CHASE parallel execution
       → [optional EV-BLP] P₁ Order → P₂ Exec → P₃ Commit
       → CacheOverlayBackend (L1 visibility) + PersistableCMemoryBackend (memory + RocksDB)
```

With `CHASE_USE_EV_BLP=1`, ordered batches flow through a three-stage concurrent pipeline instead of blocking on each batch's full execute-and-commit cycle. Uncommitted L1 state is visible to subsequent batches via `CacheOverlayBackend` on the EVM read path.

### CDS scheduling (Conflict Detection Scheduling)

CHASE maps commit waves onto two CDS zones:

| CHASE commit wave | CDS zone | Behavior |
|-------------------|----------|----------|
| Wave 1 (first commit wave) | **Conflict-free zone** | Extract parallel dependency chains; serial within a chain, parallel across chains |
| Wave 2+ (finalized) | **Conflict zone** | Inter-epoch reordering (minimum non-conflicting epoch placement) |
| Aborted transactions | **Conflict zone** | Same reordering, then re-execute |

Production commit order in `_execute`:

1. `_commit_cds_conflict_free_zone` — chains in parallel, txs in a chain serially
2. `_concurrent_commit` on conflict-zone finalized epochs
3. Re-execute conflict-zone aborted transactions

Use `_commit_cds_schedule` (not `scheduled_txs()` + `_concurrent_commit`) when committing a `ScheduledInfo` manually.

CDS scheduling runs inside **P₂ Exec** (`ConcurrencyLevelManager::execute_batch_with_effects`). EV-BLP does not replace CDS; it pipelines multiple batches so P₂, P₃, and ordering can overlap across batches.

### EV-BLP (Execute-Validate Batch-Level Pipelining)

EV-BLP overlaps batch ordering, CDS execution, and RocksDB commit across concurrent batches. Implementation lives under `crates/sslab-execution/chase/src/executor/`.

#### Three-stage pipeline

| Stage | Name | Work |
|-------|------|------|
| P₁ | Order | Allocate `batch_id`, apply AIMD window, enqueue to exec worker |
| P₂ | Exec | CDS parallel execution via `execute_batch_with_effects`; collect real `Apply` write sets into `TempBuffer` → `DeltaPage`; update L1 visibility |
| P₃ | Commit | `spawn_blocking` + `commit_batch`: L1 → L2 → RocksDB; on flush failure, frozen tables stay in L1 |

Stages communicate through Tokio channels. `PipelineController` (AIMD) limits in-flight batches per stage; cold start uses ζ = 1 for each stage.

```text
Consensus batches
      │
      ▼
  P₁ Order ──channel──► P₂ Exec (CDS + effect capture)
      │                        │
      │                        ├──► L1Visibility (visible_batch)
      │                        │
      │                        └──channel──► P₃ Commit (async RocksDB flush)
      │
      └── AIMD backpressure (W₂ gas, W₃ bytes)
```

#### Two-level cache

| Layer | Structure | Role |
|-------|-----------|------|
| L1 | `MemIndexTable` + `DeltaPage` (≤ 128 records/page) | Uncommitted writes; frozen when capacity threshold (default 256 MB) is reached |
| L2 | LRU (default 1 GB) | Committed pages awaiting or after flush |
| Store | `StateStore` trait | `RocksDbStateStore` in production; `InMemoryStateStore` in unit tests |

Read path: `CacheOverlayBackend` checks L1 (up to `visible_batch`) before falling through to the inner backend and RocksDB.

#### Module layout

```text
crates/sslab-execution/chase/src/executor/
├── config.rs              # PipelineConfig, CacheConfig, EvBlpConfig
├── runtime.rs             # EvBlpRuntime (shared pipeline + L1Visibility + metrics)
├── chase_bridge.rs        # EvBlpChaseBridge — concurrent P₁/P₂/P₃ workers
├── cache/
│   ├── delta_page.rs      # DeltaPage (key/value pages)
│   ├── mem_index_table.rs
│   ├── l1_cache.rs, l2_cache.rs
│   ├── two_level_cache.rs # L1 → L2 → RocksDB with safe flush
│   ├── cache_overlay.rs   # CacheOverlayBackend + L1Visibility
│   └── apply_buffer.rs    # Apply → TempBuffer conversion
└── pipeline/
    ├── controller.rs      # PipelineController (AIMD)
    ├── stages.rs          # StageId, PipelineBatch
    ├── ev_blp.rs          # EvBlpPipeline orchestrator
    ├── metrics.rs         # PipelineMetrics
    └── calibration.rs     # recommend_lambdas()

crates/sslab-execution/stack/
├── pipeline.rs            # ChaseStack::open — creates EvBlpRuntime when enabled
├── execution_state.rs     # Routes to EvBlpChaseBridge when runtime is present
└── rocksdb_state_store.rs # RocksDB StateStore backend
```

#### Stack integration

`ChaseStack::open` reads `CHASE_USE_EV_BLP` at open time. When enabled:

1. `EvBlpRuntime` is created with `RocksDbStateStore`.
2. `CacheOverlayBackend` wraps `PersistableCMemoryBackend` with shared `L1Visibility`.
3. `ChaseExecutionState` uses `EvBlpChaseBridge::with_runtime` instead of direct `chase.execute`.

**Important:** set `CHASE_USE_EV_BLP=1` (and other `CHASE_*` pipeline/cache variables) **before** calling `ChaseStack::open` or starting `narwhal-node`. The runtime is constructed once at stack initialization.

### Node deployment

```bash
# Tusk consensus instead of Bullshark
export CHASE_USE_TUSK=1

# CHASE + CDS execution with RocksDB under {store}/chase
export CHASE_USE_EXECUTION=1

# Optional: parallel execution width (default 4)
export CHASE_CONCURRENCY_LEVEL=4

# Optional: EV-BLP batch-level pipelining (P₁ Order → P₂ Exec → P₃ Commit; set before node / ChaseStack::open)
export CHASE_USE_EV_BLP=1
export CHASE_PIPELINE_ZETA_MAX=8
export CHASE_PIPELINE_LAMBDA2=10000000
export CHASE_PIPELINE_LAMBDA3=67108864
export CHASE_CACHE_L1_CAPACITY_MB=256
export CHASE_CACHE_DELTAPAGE_MAX_RECORDS=128
export CHASE_CACHE_L2_LRU_SIZE_MB=1024

# Start primary (store path is the Narwhal node store)
narwhal-node primary -s /path/to/store ...
```

### Configuration reference

| Variable | Default | Description |
|----------|---------|-------------|
| `CHASE_USE_EV_BLP` | off | Enable EV-BLP (`1` or `true`) |
| `CHASE_PIPELINE_ZETA_MAX` | `8` | Maximum concurrent batches per stage (ζ_max) |
| `CHASE_PIPELINE_LAMBDA2` | `10000000` | P₂ overload threshold W₂ (gas units) |
| `CHASE_PIPELINE_LAMBDA3` | `67108864` (64 MB) | P₃ overload threshold W₃ (bytes) |
| `CHASE_CACHE_L1_CAPACITY_MB` | `256` | L1 freeze threshold (MB) |
| `CHASE_CACHE_DELTAPAGE_MAX_RECORDS` | `128` | Max records per `DeltaPage` |
| `CHASE_CACHE_L2_LRU_SIZE_MB` | `1024` | L2 LRU capacity (MB) |

After a pipeline run, `PipelineMetrics` and `recommend_lambdas()` can suggest tuned `lambda2` / `lambda3` values based on observed gas and delta-byte workloads.

### Application integration

```rust
use sslab_execution_stack::ChaseStack;

// Set CHASE_USE_EV_BLP=1 in the environment before open when using EV-BLP.
let stack = ChaseStack::open("/path/to/rocksdb", concurrency_level)?;
let execution_state = stack.into_execution_state();
// Pass execution_state to narwhal_executor::Executor::spawn(...)
```

### Tests

```bash
export CC=gcc CXX=g++
export RUSTFLAGS="-C linker=g++"

# CHASE CDS + Seer unit tests
cargo test -p sslab-execution-chase --features chase

# EV-BLP executor tests (pipeline, cache, overlay)
cargo test -p sslab-execution-chase --features chase executor::

# Full stack E2E (Tusk → CHASE CDS → RocksDB)
cargo test -p sslab-execution-stack --test e2e_integration -- --test-threads=1

# EV-BLP full stack E2E
CHASE_USE_EV_BLP=1 cargo test -p sslab-execution-stack --test e2e_integration e2e_ev_blp_pipeline_execution -- --test-threads=1
```

## How to benchmark?

Note that we only use the code under `crates/sslab-execution`. All other directories are unrelated to CHASE and are not compiled by default.

#### 1. Install dependencies

```bash
sudo apt-get update
sudo apt-get -y upgrade
sudo apt-get -y autoremove
sudo apt-get -y install build-essential cmake curl clang pkg-config libssl-dev protobuf-compiler git

curl --proto "=https" --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source $HOME/.cargo/env
rustup update
rustup default stable
```

#### 2. Run benchmarks

The workload for CHASE throughput benchmarks is the SmallBank workload.

```bash
cd crates/sslab-execution/chase/benches
cargo bench -- blocksize > baseline.log
```

#### 3. EV-BLP benchmark and lambda calibration

Compares direct CHASE execution vs. the EV-BLP pipeline on SmallBank, then prints suggested `CHASE_PIPELINE_LAMBDA2` / `CHASE_PIPELINE_LAMBDA3` from `recommend_lambdas()`:

```bash
export CC=gcc CXX=g++
export RUSTFLAGS="-C linker=g++"

cargo bench -p sslab-execution-chase --features ev-blp --bench ev_blp -- --sample-size 10
```

Criterion requires `--sample-size` ≥ 10. The calibration block at the end of the benchmark is not timed; look for `Suggested env: CHASE_PIPELINE_LAMBDA2=...` in the output.

#### 4. Parsing the results

```bash
# required python version >= 3.10
python3 parse_log.py chase-tps.log  # output filename is 'chase.log.out'
```
