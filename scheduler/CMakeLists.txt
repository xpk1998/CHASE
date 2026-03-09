# Scheduler library for ParaChain
cmake_minimum_required(VERSION 3.20)

# Project definition
project(parachain-scheduler VERSION 1.0.0 LANGUAGES C CXX)

# Collect all source files
file(GLOB_RECURSE SCHEDULER_SOURCES
    "basic_sequence/*.cpp"
    "enhanced_sequence/*.cpp"
    "shard_optimizer/*.cpp"
    "parallel_optimizer/*.cpp"
    "lcdps/*.cpp"
    "optme/*.cpp"
    "serial/*.cpp"
)

# Create the scheduler library
add_library(scheduler ${SCHEDULER_SOURCES})

# Link required libraries
target_link_libraries(scheduler
    PUBLIC
        framework
        utilities
        executor
        # consensus  # 暂时注释掉consensus模块依赖
)

# Include directories
target_include_directories(scheduler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/basic_sequence>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/enhanced_sequence>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shard_optimizer>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/parallel_optimizer>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lcdps>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/optme>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/serial>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../framework>
        $<INSTALL_INTERFACE:include>
)

# Compile features
target_compile_features(scheduler PUBLIC cxx_std_20)

# Installation rules
install(TARGETS scheduler
    EXPORT parachainTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY 
    basic_sequence/
    enhanced_sequence/
    shard_optimizer/
    parallel_optimizer/
    lcdps/
    optme/
    serial/
    DESTINATION include/parachain/scheduler
    FILES_MATCHING PATTERN "*.h"
)
