# CMakeLists.txt for ffvoice-engine Tests
cmake_minimum_required(VERSION 3.15)

# Enable testing
enable_testing()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Google Test configuration
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include Google Mock for mocking
include(GoogleTest)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# Test source files
set(TEST_SOURCES
    test_main.cpp
    unit/test_wav_writer.cpp
    unit/test_flac_writer.cpp
    unit/test_signal_generator.cpp
    unit/test_audio_converter.cpp
    unit/test_vad_segmenter.cpp
    unit/test_rnnoise_processor.cpp
    unit/test_whisper_processor.cpp
    unit/test_logger.cpp
    integration/test_audio_pipeline.cpp
    # Add more test files as they are created
    # unit/test_audio_capture.cpp
    # unit/test_audio_file_writer.cpp
    # unit/test_ring_buffer.cpp
)

# Create test executable
add_executable(ffvoice_tests ${TEST_SOURCES})

# Link against Google Test, Google Mock, and the main library
target_link_libraries(ffvoice_tests
    gtest
    gtest_main
    gmock
    gmock_main
    ffvoice-core  # Link to core library
)

# Compiler options for tests
target_compile_options(ffvoice_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g -O0 --coverage>
    $<$<CONFIG:Release>:-O3>
)

# Linker options for code coverage
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(ffvoice_tests PRIVATE --coverage)
endif()

# Platform-specific configurations
if(APPLE)
    # macOS specific settings
    target_link_libraries(ffvoice_tests
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
elseif(UNIX)
    # Linux specific settings
    find_package(ALSA)
    if(ALSA_FOUND)
        target_link_libraries(ffvoice_tests ${ALSA_LIBRARIES})
        target_include_directories(ffvoice_tests PRIVATE ${ALSA_INCLUDE_DIRS})
    endif()
elseif(WIN32)
    # Windows specific settings
    target_link_libraries(ffvoice_tests winmm)
endif()

# Discover tests for CTest
gtest_discover_tests(ffvoice_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "unit"
)

# Add custom test target with verbose output
add_custom_target(test_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --output-on-failure
    DEPENDS ffvoice_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Code coverage target (requires lcov)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_target(coverage
        COMMAND lcov --capture --directory . --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' '*/tests/*' '*/googletest/*' --output-file coverage.info
        COMMAND lcov --list coverage.info
        COMMAND genhtml coverage.info --output-directory coverage_html
        DEPENDS ffvoice_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report"
    )
endif()

# Memory leak detection with valgrind (Linux only)
if(UNIX AND NOT APPLE)
    find_program(VALGRIND valgrind)
    if(VALGRIND)
        add_custom_target(test_memcheck
            COMMAND ${VALGRIND} --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1
                    $<TARGET_FILE:ffvoice_tests>
            DEPENDS ffvoice_tests
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Running memory leak detection with Valgrind"
        )
    endif()
endif()

# Performance benchmarking target
add_custom_target(benchmark
    COMMAND $<TARGET_FILE:ffvoice_tests> --gtest_filter=*Benchmark*
    DEPENDS ffvoice_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running performance benchmarks"
)

# Install test data files
file(GLOB TEST_DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
file(COPY ${TEST_DATA_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)
