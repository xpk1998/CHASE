# Utilities framework CMakeLists.txt

# Install headers
file(GLOB_RECURSE FRAMEWORK_UTILITIES_HEADERS "*.h" "*.hpp")

add_library(parachain-framework-utilities INTERFACE)

target_sources(parachain-framework-utilities
    INTERFACE
        ${FRAMEWORK_UTILITIES_HEADERS}
)

target_include_directories(parachain-framework-utilities
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)