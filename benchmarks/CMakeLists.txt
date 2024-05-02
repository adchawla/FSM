cmake_minimum_required(VERSION 3.23)

add_executable (benchmarks
    benchFSMWithEnums.cpp
    benchFSMWithStatePattern.cpp
    benchFSMStateTransitions.cpp
)

target_compile_definitions(benchmarks PUBLIC
    DISABLE_TIMEOUT_MANAGER=1
)

include(FetchContent)
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG bc946b919cac6f25a199a526da571638cfde109f
)

FetchContent_MakeAvailable(googlebenchmark)

# Link Google Benchmark to the project
target_link_libraries(benchmarks
    common
    benchmark::benchmark_main
)

# Link Shlwapi to the project
if ("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
    target_link_libraries(benchmarks Shlwapi)
endif()

