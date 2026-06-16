# CHASE

**CHASE** (Conflict-aware High-performance Adaptive Scheduling Engine) is a blockchain execution system that combines deterministic concurrency control with **CDS** (Conflict Detection Scheduling).


## Full stack

The `sslab-execution-stack` crate wires the three layers together:

```text
Transactions → Narwhal Worker → Tusk consensus → ordered batches
       → ChaseExecutionState → CDS scheduling + CHASE parallel execution
       → PersistableCMemoryBackend (memory + RocksDB write-through)
```

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

### Node deployment

```bash
# Tusk consensus instead of Bullshark
export CHASE_USE_TUSK=1

# CHASE + CDS execution with RocksDB under {store}/chase
export CHASE_USE_EXECUTION=1

# Optional: parallel execution width (default 4)
export CHASE_CONCURRENCY_LEVEL=4

# Optional: EV-BLP batch-level pipelining (P₁ Order → P₂ Exec → P₃ Commit)
export CHASE_USE_EV_BLP=1
export CHASE_PIPELINE_ZETA_MAX=8
export CHASE_PIPELINE_LAMBDA2=10000000
export CHASE_PIPELINE_LAMBDA3=67108864
export CHASE_CACHE_L1_CAPACITY_MB=256
export CHASE_CACHE_L2_LRU_SIZE_MB=1024

# Start primary (store path is the Narwhal node store)
narwhal-node primary -s /path/to/store ...
```

### Application integration

```rust
use sslab_execution_stack::ChaseStack;

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

# EV-BLP executor tests
cargo test -p sslab-execution-chase --features chase executor::

# Full stack E2E (Tusk → CHASE CDS → RocksDB)
cargo test -p sslab-execution-stack --test e2e_integration -- --test-threads=1

# EV-BLP full stack E2E
CHASE_USE_EV_BLP=1 cargo test -p sslab-execution-stack --test e2e_integration e2e_ev_blp_pipeline_execution -- --test-threads=1

# EV-BLP benchmark + lambda calibration (prints suggested lambda2/lambda3)
cargo bench -p sslab-execution-chase --features ev-blp --bench ev_blp -- --sample-size 10
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

The workload for benchmarks is the SmallBank workload.

```bash
cd crates/sslab-execution/chase/benches
cargo bench -- blocksize > baseline.log
```

#### 3. Parsing the results

```bash
# required python version >= 3.10
python3 parse_log.py chase-tps.log  # output filename is 'chase.log.out'
```
