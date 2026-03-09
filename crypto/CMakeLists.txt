# Crypto library for ParaChain
cmake_minimum_required(VERSION 3.20)

# Project definition
project(parachain-crypto VERSION 1.0.0 LANGUAGES C CXX)

# Collect all source files
file(GLOB_RECURSE CRYPTO_SOURCES
    "digestsign/*.cpp"
    "encrypt/*.cpp"
    "hash/*.cpp"
    "hasher/*.cpp"
    "interfaces/crypto/*.cpp"
    "merkle/*.cpp"
    "merkle_tree/*.cpp"
    "msp/*.cpp"
    "signature/*.cpp"
    "signature/secp256k1/*.cpp"
    "util/*.cpp"
)

# 移除ZKP相关的源文件
list(FILTER CRYPTO_SOURCES EXCLUDE REGEX ".*zkp.*")
list(FILTER CRYPTO_SOURCES EXCLUDE REGEX ".*discretezkp.*")
list(FILTER CRYPTO_SOURCES EXCLUDE REGEX ".*DiscreteLogarithmZkp.*")

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(TBB REQUIRED)

# Conditionally exclude HSM and Wedpr related files when ONLY_CPP_SDK is defined
if(ONLY_CPP_SDK)
    list(REMOVE_ITEM CRYPTO_SOURCES 
        ${CMAKE_CURRENT_SOURCE_DIR}/encrypt/HsmSM4Crypto.cpp)
endif()

# Create the crypto library
add_library(crypto ${CRYPTO_SOURCES})

# Link libraries
target_link_libraries(crypto
    PUBLIC
        framework
        utilities
)

# Include directories
target_include_directories(crypto
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interfaces>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/merkle_tree>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/msp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/util>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/utilities>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/framework>
        $<INSTALL_INTERFACE:include>
)

# Compile features
target_compile_features(crypto PUBLIC cxx_std_20)
target_compile_definitions(crypto PUBLIC -DONLY_CPP_SDK)

# Disable unity build to ensure conditional compilation works correctly
set_target_properties(crypto PROPERTIES UNITY_BUILD "OFF")

if(NOT WIN32)
    target_compile_options(crypto PUBLIC "-Wno-ignored-attributes")
endif()

# Installation rules
install(TARGETS crypto
    EXPORT parachainTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY 
    interfaces/
    merkle_tree/
    msp/
    util/
    DESTINATION include/parachain/crypto
    FILES_MATCHING PATTERN "*.h"
)