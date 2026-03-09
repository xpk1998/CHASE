# Framework library for ParaChain
cmake_minimum_required(VERSION 3.20)

project(parachain-framework VERSION 1.0.0 LANGUAGES C CXX)

# Collect all source files
file(GLOB_RECURSE FRAMEWORK_SOURCES
    "*.cpp"
    "*/protocol/*.cpp"
    "*/codec/*.cpp"
    "*/zmq/*.cpp"
)

# Find required packages
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# Create the framework library
add_library(framework ${FRAMEWORK_SOURCES})

# Link libraries
target_link_libraries(framework
    PUBLIC
        utilities
        protobuf::libprotobuf
        ${ZMQ_LIBRARIES}
)

# Include directories
target_include_directories(framework
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Compile features
target_compile_features(framework PUBLIC cxx_std_20)