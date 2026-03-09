cmake_minimum_required(VERSION 3.10)
project(CHASE VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# Find required packages
find_package(GTest REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem thread)
find_package(TBB REQUIRED)
find_package(PkgConfig REQUIRED)
# find_package(yaml-cpp REQUIRED)
# Use pkg-config instead
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)
find_package(glog REQUIRED)

# Find ZeroMQ
pkg_check_modules(ZMQ REQUIRED libzmq)

# Find JsonCpp - use pkg-config as fallback
find_package(jsoncpp QUIET)
if(NOT jsoncpp_FOUND)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
endif()

# Generate Protocol Buffers files
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto_gen)
set(PROTOBUF_IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto)
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")

find_program(PROTOC_EXECUTABLE protoc)
if(PROTOC_EXECUTABLE AND PROTO_FILES)
    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        set(PROTO_OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/proto_gen/${PROTO_NAME}.pb.h")
        add_custom_command(
            OUTPUT ${PROTO_OUTPUT_FILE}
            COMMAND ${PROTOC_EXECUTABLE}
            ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/proto_gen -I${PROTOBUF_IMPORT_DIRS} ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating ${PROTO_NAME}.pb.h"
            VERBATIM
        )
        list(APPEND PROTO_GEN_FILES ${PROTO_OUTPUT_FILE})
    endforeach()
    
    add_custom_target(generate_proto ALL DEPENDS ${PROTO_GEN_FILES})
    
    # Add proto_gen directory to include directories for all targets
    include_directories(${CMAKE_CURRENT_BINARY_DIR}/proto_gen)
endif()

# Add subdirectories
add_subdirectory(concepts)
add_subdirectory(framework)
add_subdirectory(crypto)
add_subdirectory(storage)
add_subdirectory(network)
add_subdirectory(executor)
add_subdirectory(scheduler)
add_subdirectory(consensus)
add_subdirectory(runtime)
add_subdirectory(txpool)

# Main executable
add_executable(CHASE_node 
    main.cpp
)

# Link libraries
target_link_libraries(CHASE_node
    PRIVATE
        framework
        crypto
        storage
        network
        executor
        scheduler
        consensus
        runtime
        txpool
        glog::glog
        OpenSSL::SSL
        OpenSSL::Crypto
        ${ZMQ_LIBRARIES}
        Boost::system
        Boost::filesystem
        Boost::thread
        protobuf::libprotobuf
        ${YAML_CPP_LIBRARIES}
)

# Add JsonCpp library to main executable
if(TARGET jsoncpp_static)
    target_link_libraries(CHASE_node PRIVATE jsoncpp_static)
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(CHASE_node PRIVATE JsonCpp::JsonCpp)
elseif(JSONCPP_LIBRARIES)
    target_link_libraries(CHASE_node PRIVATE ${JSONCPP_LIBRARIES})
    target_include_directories(CHASE_node PRIVATE ${JSONCPP_INCLUDE_DIRS})
endif()

# Include directories
target_include_directories(CHASE_node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/concepts
    ${CMAKE_CURRENT_SOURCE_DIR}/CHASE-framework
    ${CMAKE_CURRENT_BINARY_DIR}/proto_gen
)

# Set global include directories for all subdirectories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/concepts
    ${CMAKE_CURRENT_SOURCE_DIR}/utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/CHASE-framework
    ${CMAKE_CURRENT_BINARY_DIR}/proto_gen
)