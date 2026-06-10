# CHASE

**CHASE** (Conflict-aware High-performance Adaptive Scheduling Engine) is a blockchain execution system that combines deterministic concurrency control with **CDS** (Conflict Detection Scheduling).

The execution engine builds on ideas from the SC '24 paper [*Toward High-Performance Blockchain System by Blurring the Line between Ordering and Execution*](https://dl.acm.org/doi/10.1145/3650212.3652144) (originally published as OptME), extended with CDS two-zone scheduling and Seer-accelerated pre-execution.

## Full stack (Tusk + CHASE + RocksDB)

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

### Seer-accelerated simulation (VLDB 2025)

CHASE's pre-execution phase (RW-set extraction for KDG construction) uses [Seer](https://www.vldb.org/pvldb/vol18/p822-xiao.pdf)-inspired acceleration:

| Component | Role |
|-----------|------|
| **PreExecutionCache** | Reuse prior simulation RW sets / effects (checkpoint fast-path) |
| **VarTable + Perceptron** | Two-level branch-direction learning across transactions |
| **Contract-locality ordering** | Warm predictor before parallel simulation |

```bash
export CHASE_USE_SEER=1              # default on; set 0 to disable
export CHASE_SEER_CACHE=1            # pre-execution result cache
export CHASE_SEER_PERCEPTRON=1       # branch predictor learning
export CHASE_SEER_LOCALITY=1         # sort txs by contract for warmup
```

Reference: [SeerEVM](https://github.com/CGCL-codes/SeerEVM). Fine-grained EVM `JUMPI` hooks will be integrated in `crates/chase-evm` in a follow-up.

### EVM fork (`chase-evm`)

The CHASE EVM interpreter is vendored at `crates/chase-evm` (lineage: `optme-evm` @ `d81889d`). The workspace pins it via a path dependency instead of the legacy `optme-evm` git URL.

### Pull request merge order

Three feature branches stack on `main`; each later branch contains all commits from earlier ones:

```text
main
 └── cursor/cds-scheduling-cb3b     (#4 — CDS two-zone scheduling)
      └── cursor/seer-integration-cb3b (#5 — Seer-accelerated simulation)
           └── cursor/chase-rename-cb3b (#6 — OptME → CHASE rename + chase-evm)
```

| PR | Branch | Scope |
|----|--------|-------|
| [#4](https://github.com/xpk1998/optme/pull/4) | `cursor/cds-scheduling-cb3b` | CDS conflict-free + conflict zones |
| [#5](https://github.com/xpk1998/optme/pull/5) | `cursor/seer-integration-cb3b` | Seer cache / perceptron / locality |
| [#6](https://github.com/xpk1998/optme/pull/6) | `cursor/chase-rename-cb3b` | CHASE rename, `chase-evm` vendoring, docs |

**Recommended:** merge **#6 only** into `main`, then close #4 and #5 as superseded (their commits are already included).

**Alternative (incremental review):** merge #4 → rebase #5 onto `main` → merge #5 → rebase #6 onto `main` → merge #6. Do not merge #4 and #6 in parallel — that would duplicate commits.

### Tests

```bash
export CC=gcc CXX=g++
export RUSTFLAGS="-C linker=g++"

# CHASE CDS + Seer unit tests
cargo test -p sslab-execution-chase --features chase

# Full stack E2E (Tusk → CHASE CDS → RocksDB)
cargo test -p sslab-execution-stack --test e2e_integration -- --test-threads=1
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
