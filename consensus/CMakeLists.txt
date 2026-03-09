# Consensus module CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# Try to find braft
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(BRAFT QUIET libbraft)
endif()

# If braft not found, disable braft-dependent components
if(NOT BRAFT_FOUND)
    message(STATUS "braft not found, disabling braft-dependent components")
    # Remove braft-dependent sources from the glob
    file(GLOB_RECURSE CONSENSUS_SOURCES
        "core/*.cpp"
        "core/comm/*.cpp"
        "core/coordinator/*.cpp"
        "core/core/*.cpp"
        "pbft/cache/*.cpp"
        "pbft/config/*.cpp"
        "pbft/engine/*.cpp"
        "pbft/interfaces/*.cpp"
        "pbft/protocol/*.cpp"
        "pbft/storage/*.cpp"
        "pbft/utilities/*.cpp"
        "pbft/core/*.cpp"
        "aptos/*.cpp"
        "network/*.cpp"
    )
    
    # Remove braft-dependent files
    file(GLOB BRAFT_DEPENDENT_SOURCES
        "core/raft_chain.cpp"
        "core/core/consensus_factory.cpp"
    )
    
    # Filter out braft-dependent sources
    list(REMOVE_ITEM CONSENSUS_SOURCES ${BRAFT_DEPENDENT_SOURCES})
else()
    message(STATUS "braft found, enabling braft-dependent components")
    # Collect all source files
    file(GLOB_RECURSE CONSENSUS_SOURCES
        "core/*.cpp"
        "core/comm/*.cpp"
        "core/coordinator/*.cpp"
        "core/core/*.cpp"
        "pbft/cache/*.cpp"
        "pbft/config/*.cpp"
        "pbft/engine/*.cpp"
        "pbft/interfaces/*.cpp"
        "pbft/protocol/*.cpp"
        "pbft/storage/*.cpp"
        "pbft/utilities/*.cpp"
        "pbft/core/*.cpp"
        "aptos/*.cpp"
        "network/*.cpp"
    )
endif()

# Add consensus library
add_library(consensus ${CONSENSUS_SOURCES})

# Link required libraries
target_link_libraries(consensus
    PUBLIC
        framework
        utilities
        crypto
        executor
        scheduler
        $<$<BOOL:${BRAFT_FOUND}>:${BRAFT_LIBRARY}>
        $<$<BOOL:${BRAFT_FOUND}>:${BRPC_LIBRARY}>
)

# Include directories
target_include_directories(consensus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/comm
    ${CMAKE_CURRENT_SOURCE_DIR}/core/coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/core/core
    ${CMAKE_CURRENT_SOURCE_DIR}/pbft
    ${CMAKE_CURRENT_SOURCE_DIR}/pbft/cache
    ${CMAKE_CURRENT_SOURCE_DIR}/pbft/config
    ${CMAKE_CURRENT_SOURCE_DIR}/pbft/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/pbft/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/pbft/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/pbft/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/pbft/utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/pbft/core
    ${CMAKE_CURRENT_SOURCE_DIR}/aptos
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_SOURCE_DIR}/crypto
    ${CMAKE_SOURCE_DIR}/executor
    ${CMAKE_SOURCE_DIR}/runtime/include
    ${CMAKE_SOURCE_DIR}/scheduler/include
    ${CMAKE_SOURCE_DIR}/framework
    ${CMAKE_BINARY_DIR}  # 添加build目录以解决protobuf包含问题
    $<$<BOOL:${BRAFT_FOUND}>:${BRAFT_INCLUDE_DIR}>
    $<$<BOOL:${BRAFT_FOUND}>:${BRPC_INCLUDE_DIR}>
)