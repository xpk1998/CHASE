# Executor library for ParaChain
cmake_minimum_required(VERSION 3.20)

# Project definition
project(parachain-executor VERSION 1.0.0 LANGUAGES C CXX)

# Collect all source files
file(GLOB_RECURSE EXECUTOR_SOURCES
    "core/*.cpp"
    "pipeline/*.cpp"
    "kdg/*.cpp"
    "impl/*.cpp"
    "concurrency/*.cpp"
    "concurrent/*.cpp"
    "payload/*.cpp"
    "statistics/*.cpp"
    "transaction/*.cpp"
    "utils/*.cpp"
    "blp/*.cpp"
)

# 移除包含monitor目录的源文件，因为它们依赖于consensus模块
list(FILTER EXECUTOR_SOURCES EXCLUDE REGEX ".*monitor.*")

# Create the executor library
add_library(executor ${EXECUTOR_SOURCES})

# Link required libraries
target_link_libraries(executor
    PUBLIC
        framework
        utilities
        storage
        scheduler
)

# Include directories
target_include_directories(executor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/conflict>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/dependency>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/reserve>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/scheduler>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/transaction>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pipeline>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/kdg>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/impl>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/concurrency>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/concurrent>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/payload>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/statistics>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/transaction>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/blp>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/storage>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/storage/core>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/storage/core/cache>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/framework>
        $<INSTALL_INTERFACE:include>
)

# Compile features
target_compile_features(executor PUBLIC cxx_std_20)

# Installation rules
install(TARGETS executor
    EXPORT parachainTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY 
    core/
    pipeline/
    kdg/
    impl/
    concurrency/
    concurrent/
    payload/
    statistics/
    transaction/
    utils/
    blp/
    DESTINATION include/parachain/executor
    FILES_MATCHING PATTERN "*.h"
)