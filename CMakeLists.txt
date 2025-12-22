# ffvoice-engine - Low-latency C++ voice engine
cmake_minimum_required(VERSION 3.20)
project(ffvoice-engine VERSION 0.1.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(ENABLE_WEBRTC_APM "Enable WebRTC Audio Processing Module" OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
endif()

# Find dependencies
find_package(PkgConfig REQUIRED)

# FFmpeg (required)
pkg_check_modules(FFMPEG REQUIRED
    libavcodec
    libavformat
    libavutil
    libswresample
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
)

# Source files
set(FFVOICE_CORE_SOURCES
    src/audio/audio_capture_device.cpp
    src/media/audio_file_writer.cpp
    src/media/wav_writer.cpp
    src/utils/logger.cpp
    src/utils/ring_buffer.cpp
    src/utils/signal_generator.cpp
)

# Core library
add_library(ffvoice-core STATIC ${FFVOICE_CORE_SOURCES})

target_link_libraries(ffvoice-core
    PUBLIC
        ${FFMPEG_LINK_LIBRARIES}
)

target_link_directories(ffvoice-core
    PUBLIC
        ${FFMPEG_LIBRARY_DIRS}
)

target_include_directories(ffvoice-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# CLI application
add_executable(ffvoice apps/cli/main.cpp)
target_link_libraries(ffvoice PRIVATE ffvoice-core)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(BUILD_EXAMPLES)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt)
        add_subdirectory(examples)
    endif()
endif()

# Install
install(TARGETS ffvoice ffvoice-core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ffvoice DESTINATION include)
