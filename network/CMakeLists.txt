# Network module CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem thread)
find_package(OpenSSL REQUIRED)
find_package(glog REQUIRED)

# Find ZeroMQ
pkg_check_modules(ZMQ REQUIRED libzmq)

# Collect only core network sources
file(GLOB_RECURSE NETWORK_SOURCES
    "core/*.cpp"
    "zmq/*.cpp"
)

# Create the network library
add_library(network ${NETWORK_SOURCES})

# Link libraries
target_link_libraries(network
    PUBLIC
        framework
        utilities
        glog::glog
        ${ZMQ_LIBRARIES}
        Boost::system
        Boost::filesystem
        Boost::thread
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Include directories
target_include_directories(network
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/message
        ${CMAKE_CURRENT_SOURCE_DIR}/core/discovery
        ${CMAKE_CURRENT_SOURCE_DIR}/core/router
        ${CMAKE_CURRENT_SOURCE_DIR}/zmq
        ${CMAKE_CURRENT_SOURCE_DIR}/../framework
)

# Compile features
target_compile_features(network PUBLIC cxx_std_20)