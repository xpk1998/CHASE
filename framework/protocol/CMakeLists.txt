# Protocol framework CMakeLists.txt

# Install headers
file(GLOB_RECURSE FRAMEWORK_PROTOCOL_HEADERS "*.h" "*.hpp")

add_library(parachain-framework-protocol INTERFACE)

target_sources(parachain-framework-protocol
    INTERFACE
        ${FRAMEWORK_PROTOCOL_HEADERS}
)

target_include_directories(parachain-framework-protocol
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)