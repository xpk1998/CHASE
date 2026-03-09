# CHASE - VLDB Artifact

CHASE is a high-performance blockchain execution engine with parallel execution architecture for efficient transaction processing.

## VLDB Artifact Minimization

**Retained Core Components:**
- ✓ executor/kdg/  - KDG execution engine algorithms
- ✓ executor/pipeline/  - Parallel execution framework  
- ✓ consensus/core/coordinator/ - Scheduling strategies
- ✓ storage/core/ - Core storage implementations

## Building and Running

### Prerequisites

- CMake 3.10+
- C++20 compiler
- Protocol Buffers
- OpenSSL
- Boost, TBB, glog, ZeroMQ, JsonCpp

### Build

```bash
mkdir build && cd build
cmake ..
make
```

### Run Performance Tests

```bash
./test_performance.sh
```

## Code Organization

- **executor/**: Core execution engines
- **consensus/**: Scheduling and coordination
- **storage/**: Data persistence interfaces
- **proto/**: Message definitions
- **framework/**: Core abstractions

This minimized artifact focuses on the algorithmic contributions while providing sufficient code for verification.

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

### Build

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```


## Running

After building, run the main program:

```bash
./build/CHASE_node
```

## Project Structure

```
CHASE/
├── main.cpp           # Main program entry point
├── CMakeLists.txt     # CMake configuration
├── cmake/             # CMake modules and settings
├── proto/             # Protocol Buffer definitions
├── common/            # Common utilities and configuration
├── concepts/          # Core concepts and interfaces
├── consensus/         # Consensus module
├── crypto/            # Cryptography module
├── executor/          # Transaction execution engine
├── network/           # Network communication
├── node/              # Blockchain node
├── scheduler/         # Transaction scheduling
├── storage/           # Storage module
├── sealer/            # Block sealing
├── runtime/           # Runtime environment
└── framework/         # Framework integration layer
```
