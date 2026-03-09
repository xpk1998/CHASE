# Runtime library for ParaChain
cmake_minimum_required(VERSION 3.20)

# Project definition
project(parachain-runtime VERSION 1.0.0 LANGUAGES C CXX)

# Find all source files
file(GLOB_RECURSE RUNTIME_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vm/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vm/precompiled/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/builtin/*.cpp"
)

# Create runtime library
add_library(runtime ${RUNTIME_SOURCES})

# Link required libraries
target_link_libraries(runtime
    PUBLIC
        framework
        utilities
        crypto
        storage
        ${EVMONE_LIBRARY}
)

# Include directories
target_include_directories(runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vm>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vm/precompiled>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/builtin>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../framework>
        $<INSTALL_INTERFACE:include>
        ${EVMC_INCLUDE_DIR}
        ${EVMONE_INCLUDE_DIR}
)

# Compile features
target_compile_features(runtime PUBLIC cxx_std_20)

# Installation rules
install(TARGETS runtime
    EXPORT parachainTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY 
    vm/
    builtin/
    DESTINATION include/parachain/runtime
    FILES_MATCHING PATTERN "*.h"
)