# Concepts library for ParaChain
cmake_minimum_required(VERSION 3.20)

project(parachain-concepts VERSION 1.0.0 LANGUAGES CXX)

# Find required packages
find_package(Boost REQUIRED)

# Create the concepts library (INTERFACE since it's header-only)
add_library(concepts INTERFACE)

# Link libraries
target_link_libraries(concepts
    INTERFACE
        Boost::headers
)

# Include directories
target_include_directories(concepts
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Compile features
target_compile_features(concepts INTERFACE cxx_std_20)