# TxPool framework CMakeLists.txt

# Install headers
file(GLOB_RECURSE FRAMEWORK_TXPOOL_HEADERS "*.h" "*.hpp")

add_library(parachain-framework-txpool INTERFACE)

target_sources(parachain-framework-txpool
    INTERFACE
        ${FRAMEWORK_TXPOOL_HEADERS}
)

target_include_directories(parachain-framework-txpool
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)