# Crypto framework CMakeLists.txt

# Install headers
file(GLOB_RECURSE FRAMEWORK_CRYPTO_HEADERS "*.h" "*.hpp")

add_library(parachain-framework-crypto INTERFACE)

target_sources(parachain-framework-crypto
    INTERFACE
        ${FRAMEWORK_CRYPTO_HEADERS}
)

target_include_directories(parachain-framework-crypto
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)