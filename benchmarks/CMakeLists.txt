# CMakeLists.txt for ffvoice-engine Benchmarks
cmake_minimum_required(VERSION 3.15)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Benchmark source files
set(BENCHMARK_SOURCES
    benchmark_main.cpp
    benchmark_audio_processing.cpp
    benchmark_audio_conversion.cpp
)

# Create benchmark executable
add_executable(ffvoice_benchmarks ${BENCHMARK_SOURCES})

# Link against Google Benchmark and the main library
target_link_libraries(ffvoice_benchmarks
    benchmark::benchmark
    ffvoice-core
)

# Compiler options for benchmarks
target_compile_options(ffvoice_benchmarks PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Release>:-O3>
)

# Custom target to run benchmarks
add_custom_target(run_benchmarks
    COMMAND $<TARGET_FILE:ffvoice_benchmarks>
    DEPENDS ffvoice_benchmarks
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running performance benchmarks"
)

# Custom target to run benchmarks with JSON output
add_custom_target(run_benchmarks_json
    COMMAND $<TARGET_FILE:ffvoice_benchmarks> --benchmark_format=json --benchmark_out=benchmarks.json
    DEPENDS ffvoice_benchmarks
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running benchmarks and saving to JSON"
)
