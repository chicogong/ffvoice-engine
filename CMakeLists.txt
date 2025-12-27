# ffvoice-engine - Low-latency C++ voice engine
cmake_minimum_required(VERSION 3.20)
project(ffvoice-engine VERSION 0.1.0 LANGUAGES CXX C)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set architecture for macOS (use native architecture)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architecture" FORCE)
endif()

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(ENABLE_WEBRTC_APM "Enable WebRTC Audio Processing Module" OFF)
option(ENABLE_RNNOISE "Enable RNNoise deep learning noise suppression" OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Use -march=native only on x86_64, not on ARM
        # On macOS, check CMAKE_OSX_ARCHITECTURES instead of CMAKE_SYSTEM_PROCESSOR
        if(APPLE)
            if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64|AMD64")
                add_compile_options(-O3 -march=native)
            else()
                add_compile_options(-O3)
            endif()
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            add_compile_options(-O3 -march=native)
        else()
            add_compile_options(-O3)
        endif()
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

# PortAudio (required for audio capture)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# FLAC (required for FLAC encoding)
pkg_check_modules(FLAC REQUIRED flac)

# WebRTC APM (optional)
if(ENABLE_WEBRTC_APM)
    pkg_check_modules(WEBRTC_APM webrtc-audio-processing-1)

    if(NOT WEBRTC_APM_FOUND)
        message(STATUS "")
        message(STATUS "========================================")
        message(STATUS "WebRTC APM library not found!")
        message(STATUS "========================================")
        message(STATUS "")
        message(STATUS "To install webrtc-audio-processing:")
        message(STATUS "")
        message(STATUS "Linux (Ubuntu/Debian):")
        message(STATUS "  sudo apt-get install webrtc-audio-processing-dev")
        message(STATUS "")
        message(STATUS "macOS/Linux (from source):")
        message(STATUS "  git clone https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing.git")
        message(STATUS "  cd webrtc-audio-processing")
        message(STATUS "  meson setup build --prefix=/usr/local")
        message(STATUS "  meson compile -C build")
        message(STATUS "  sudo meson install -C build")
        message(STATUS "")
        message(STATUS "Or disable WebRTC APM and use other processors:")
        message(STATUS "  cmake .. -DENABLE_WEBRTC_APM=OFF")
        message(STATUS "")
        message(STATUS "========================================")
        message(FATAL_ERROR "WebRTC APM library required but not found")
    endif()

    message(STATUS "WebRTC APM found in system:")
    message(STATUS "  Include dirs: ${WEBRTC_APM_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${WEBRTC_APM_LIBRARIES}")
    message(STATUS "  Library dirs: ${WEBRTC_APM_LIBRARY_DIRS}")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${FLAC_INCLUDE_DIRS}
)

if(ENABLE_WEBRTC_APM AND WEBRTC_APM_FOUND)
    include_directories(${WEBRTC_APM_INCLUDE_DIRS})
endif()

# Source files
set(FFVOICE_CORE_SOURCES
    src/audio/audio_capture_device.cpp
    src/audio/audio_processor.cpp
    src/media/audio_file_writer.cpp
    src/media/wav_writer.cpp
    src/media/flac_writer.cpp
    src/utils/logger.cpp
    src/utils/ring_buffer.cpp
    src/utils/signal_generator.cpp
)

# WebRTC APM (conditional)
if(ENABLE_WEBRTC_APM)
    message(STATUS "WebRTC APM: Enabled")
    add_compile_definitions(ENABLE_WEBRTC_APM)
    list(APPEND FFVOICE_CORE_SOURCES
        src/audio/webrtc_processor.cpp
    )
else()
    message(STATUS "WebRTC APM: Disabled (use -DENABLE_WEBRTC_APM=ON to enable)")
endif()

# RNNoise (conditional)
if(ENABLE_RNNOISE)
    message(STATUS "RNNoise: Enabled")
    add_compile_definitions(ENABLE_RNNOISE)

    # Fetch RNNoise library via CMake FetchContent
    include(FetchContent)

    message(STATUS "Fetching RNNoise from GitHub...")

    FetchContent_Declare(
        rnnoise
        GIT_REPOSITORY https://github.com/xiph/rnnoise.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/rnnoise-src
    )

    # Download only, don't try to build (RNNoise uses autotools, not CMake)
    FetchContent_GetProperties(rnnoise)
    if(NOT rnnoise_POPULATED)
        FetchContent_Populate(rnnoise)
    endif()

    # RNNoise source files (manual configuration since it uses autotools)
    set(RNNOISE_SOURCES
        ${rnnoise_SOURCE_DIR}/src/rnn.c
        ${rnnoise_SOURCE_DIR}/src/rnn_data.c
        ${rnnoise_SOURCE_DIR}/src/rnn_reader.c
        ${rnnoise_SOURCE_DIR}/src/denoise.c
        ${rnnoise_SOURCE_DIR}/src/celt_lpc.c
        ${rnnoise_SOURCE_DIR}/src/kiss_fft.c
        ${rnnoise_SOURCE_DIR}/src/pitch.c
    )

    # Create RNNoise static library
    add_library(rnnoise STATIC ${RNNOISE_SOURCES})

    target_include_directories(rnnoise PUBLIC
        ${rnnoise_SOURCE_DIR}/include
    )

    # RNNoise is a C library
    set_target_properties(rnnoise PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
    )

    message(STATUS "RNNoise configured successfully")
    message(STATUS "  Source dir: ${rnnoise_SOURCE_DIR}")
    message(STATUS "  Include dir: ${rnnoise_SOURCE_DIR}/include")

    list(APPEND FFVOICE_CORE_SOURCES
        src/audio/rnnoise_processor.cpp
    )
else()
    message(STATUS "RNNoise: Disabled (use -DENABLE_RNNOISE=ON to enable)")
endif()

# Core library
add_library(ffvoice-core STATIC ${FFVOICE_CORE_SOURCES})

target_link_libraries(ffvoice-core
    PUBLIC
        ${FFMPEG_LINK_LIBRARIES}
        ${PORTAUDIO_LIBRARIES}
        ${FLAC_LIBRARIES}
)

if(ENABLE_WEBRTC_APM AND WEBRTC_APM_FOUND)
    target_link_libraries(ffvoice-core
        PUBLIC ${WEBRTC_APM_LIBRARIES}
    )
endif()

if(ENABLE_RNNOISE)
    target_link_libraries(ffvoice-core
        PUBLIC rnnoise
    )
    target_include_directories(ffvoice-core
        PUBLIC ${rnnoise_SOURCE_DIR}/include
    )
endif()

target_link_directories(ffvoice-core
    PUBLIC
        ${FFMPEG_LIBRARY_DIRS}
        ${PORTAUDIO_LIBRARY_DIRS}
        ${FLAC_LIBRARY_DIRS}
)

if(ENABLE_WEBRTC_APM AND WEBRTC_APM_FOUND)
    target_link_directories(ffvoice-core
        PUBLIC ${WEBRTC_APM_LIBRARY_DIRS}
    )
endif()

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
