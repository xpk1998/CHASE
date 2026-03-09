# Transaction Pool Module
# This CMakeLists.txt defines the txpool library for Parachain
# It follows the FISCO-BCOS design pattern with modular structure
set(SRC_TXPOOL
    # Core files
    TxPool.cpp
    TxPoolConfig.h
    
    # Validator files
    validator/TxValidator.cpp
    validator/TxPoolNonceChecker.cpp
    
    # Storage files
    storage/MemoryStorage.cpp
    
    # Sync files
    sync/TransactionSync.cpp
    
    # BLP files - only keep the interface as implementation was removed
    blp/blp_txpool_interface.h
)

# Create txpool library
add_library(txpool ${SRC_TXPOOL})

# Define alias for backward compatibility
add_library(parachain::txpool ALIAS txpool)

# Link required libraries
target_link_libraries(txpool
    PUBLIC
        framework
        utilities
        crypto
    PRIVATE
        protobuf::libprotobuf
)

# Remove duplicate linking
# target_link_libraries(txpool PRIVATE common crypto)  # Removed to avoid duplication

# Include directories
target_include_directories(txpool
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/validator
        ${CMAKE_CURRENT_SOURCE_DIR}/storage
        ${CMAKE_CURRENT_SOURCE_DIR}/sync
        ${CMAKE_CURRENT_SOURCE_DIR}/sync/interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/blp
        ${CMAKE_SOURCE_DIR}/crypto/include
        ${CMAKE_SOURCE_DIR}/framework
)