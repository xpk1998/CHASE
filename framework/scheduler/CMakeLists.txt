# Scheduler framework CMakeLists.txt

# Install headers
file(GLOB_RECURSE FRAMEWORK_SCHEDULER_HEADERS "*.h" "*.hpp")

add_library(parachain-framework-scheduler INTERFACE)

target_sources(parachain-framework-scheduler
    INTERFACE
        ${FRAMEWORK_SCHEDULER_HEADERS}
)

target_include_directories(parachain-framework-scheduler
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)