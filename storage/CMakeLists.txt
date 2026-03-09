# Storage library for ParaChain
cmake_minimum_required(VERSION 3.20)

# Project definition
project(parachain-storage VERSION 1.0.0 LANGUAGES C CXX)

# Find required packages
find_package(RocksDB QUIET)
find_package(TBB REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem thread)
find_package(Protobuf REQUIRED)

# Set RocksDB paths if not found
if(NOT RocksDB_FOUND)
    set(ROCKSDB_INCLUDE_DIR "/usr/include")
    set(ROCKSDB_LIBRARY "/usr/lib/librocksdb.so")
endif()

# Collect all source files
file(GLOB_RECURSE STORAGE_SOURCES
    "core/database/*.cpp"
    "core/block/*.cpp"
    "core/cache/*.cpp"
    "core/state/*.cpp"
    "database/impl/*.cpp"
)

# Exclude problematic files
list(FILTER STORAGE_SOURCES EXCLUDE REGEX "block_broadcaster.cpp")

# Create the storage library
add_library(storage ${STORAGE_SOURCES})

# Link libraries
target_link_libraries(storage
    PUBLIC
        framework
        utilities
        crypto
        ${Protobuf_LIBRARIES}
        $<$<TARGET_EXISTS:RocksDB::rocksdb>:RocksDB::rocksdb>
        $<$<NOT:$<TARGET_EXISTS:RocksDB::rocksdb>>:${ROCKSDB_LIBRARY}>
        TBB::tbb
        Boost::system
        Boost::filesystem
        Boost::thread
)

# Include directories
target_include_directories(storage
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/database>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/block>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/cache>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/state>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/database>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/database/impl>

        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/util>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/util>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../framework>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>  # For protobuf generated files
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/proto_gen>  # For protobuf generated files in main directory
        $<INSTALL_INTERFACE:include>
        $<$<TARGET_EXISTS:RocksDB::rocksdb>:${RocksDB_INCLUDE_DIRS}>
        $<$<NOT:$<TARGET_EXISTS:RocksDB::rocksdb>>:${ROCKSDB_INCLUDE_DIR}>
)

# Compile features
target_compile_features(storage PUBLIC cxx_std_20)

# Installation rules
install(TARGETS storage
    EXPORT parachainTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY 
    core/
    database/
    util/
    DESTINATION include/parachain/storage
    FILES_MATCHING PATTERN "*.h"
)