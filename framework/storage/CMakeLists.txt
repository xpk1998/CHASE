# Storage framework CMakeLists.txt

# Install headers
file(GLOB_RECURSE FRAMEWORK_STORAGE_HEADERS "*.h" "*.hpp")

add_library(parachain-framework-storage INTERFACE)

target_sources(parachain-framework-storage
    INTERFACE
        ${FRAMEWORK_STORAGE_HEADERS}
)

target_include_directories(parachain-framework-storage
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)