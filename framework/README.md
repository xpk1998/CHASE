# ParaChain Framework Refactoring

## Overview
This refactoring project reorganizes the ParaChain codebase to follow the FISCO-BCOS architecture pattern, with a clear separation between interfaces and implementations.

## New Architecture Structure
```
parachain-framework/
├── protocol/           # Transaction, Block, Receipt interfaces
├── consensus/          # Consensus interfaces  
├── crypto/             # Crypto interfaces
├── storage/            # Storage interfaces
├── txpool/             # Transaction pool interfaces
├── executor/           # Executor interfaces
├── scheduler/          # Scheduler interfaces
├── utilities/          # Common utilities
└── Common.h            # Common includes
```

## Refactoring Progress
- [x] Created new framework directory structure
- [x] Migrated existing interfaces to appropriate framework modules
- [x] Updated include paths in framework files
- [x] Created CMakeLists.txt for framework modules
- [ ] Update original modules to use new framework (in progress)
- [ ] Consolidate duplicate utilities
- [ ] Update all CMakeLists.txt files to link against the new framework
- [ ] Test and validate refactored code

## Key Changes Made

### 1. Protocol Module
- Moved Transaction, Block, BlockHeader, TransactionReceipt, TransactionMetaData interfaces to `parachain-framework/protocol/`
- Updated include paths to reference framework utilities and crypto modules

### 2. Crypto Module
- Moved all crypto interfaces (CryptoSuite, Hash, KeyInterface, etc.) to `parachain-framework/crypto/`

### 3. Storage Module
- Moved StorageInterface to `parachain-framework/storage/`

### 4. Consensus Module
- Moved PBFT interfaces to `parachain-framework/consensus/`

### 5. Executor Module
- Moved executor interfaces to `parachain-framework/executor/`

### 6. Scheduler Module
- Moved scheduler interfaces to `parachain-framework/scheduler/`

### 7. Utilities
- Moved common utilities to `parachain-framework/utilities/`

## Next Steps

### 1. Update Original Modules
Each original module needs to be updated to:
- Include interfaces from the new framework instead of local paths
- Remove duplicate implementations
- Update CMakeLists.txt to link against the framework

### 2. Consolidate Duplicate Code
- Identify and remove duplicate utilities across modules
- Move common implementations to the framework

### 3. Testing
- Ensure all functionality remains intact after refactoring
- Run existing tests to validate changes
- Address any compilation or runtime issues

## Benefits of This Refactoring

1. **Improved Code Organization**: Clear separation between interfaces and implementations
2. **Reduced Code Duplication**: Common utilities and interfaces centralized
3. **Better Maintainability**: Clear module boundaries and dependencies
4. **Enhanced Extensibility**: Easy to add new implementations that conform to existing interfaces
5. **Aligned with Industry Standards**: Following the proven FISCO-BCOS architecture pattern