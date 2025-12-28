# ffvoice-engine - Low-latency C++ voice engine
cmake_minimum_required(VERSION 3.20)
project(ffvoice-engine VERSION 0.1.0 LANGUAGES CXX C)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set architecture for macOS (allow override from setup.py for universal2 builds)
if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    # Default to native architecture if not specified
    set(CMAKE_OSX_ARCHITECTURES "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "macOS architecture" FORCE)
endif()

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_PYTHON "Build Python bindings" OFF)
option(ENABLE_WEBRTC_APM "Enable WebRTC Audio Processing Module" OFF)
option(ENABLE_RNNOISE "Enable RNNoise deep learning noise suppression" OFF)
option(ENABLE_WHISPER "Enable Whisper ASR (speech recognition)" OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Use -march=native only for single-architecture builds on x86_64
        # Skip for universal2 builds (multiple architectures) as -march=native is incompatible
        if(APPLE)
            # Check if building for multiple architectures (universal2)
            string(FIND "${CMAKE_OSX_ARCHITECTURES}" ";" IS_UNIVERSAL)
            if(IS_UNIVERSAL GREATER -1)
                # Universal2 build - skip -march=native
                add_compile_options(-O3)
            elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64|AMD64")
                # Single x86_64 build
                add_compile_options(-O3 -march=native)
            else()
                # Single ARM64 build
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
    src/audio/vad_segmenter.cpp
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
        POSITION_INDEPENDENT_CODE ON
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

# Whisper ASR (conditional)
if(ENABLE_WHISPER)
    message(STATUS "Whisper ASR: Enabled")
    add_compile_definitions(ENABLE_WHISPER)

    # Fetch whisper.cpp library via CMake FetchContent
    include(FetchContent)

    message(STATUS "Fetching whisper.cpp from GitHub...")

    FetchContent_Declare(
        whisper
        GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
        GIT_TAG v1.5.4
        GIT_SHALLOW TRUE
    )

    # Configure whisper.cpp build options
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # Disable AVX/AVX2/SIMD for Rosetta 2 compatibility
    set(WHISPER_NO_AVX ON CACHE BOOL "" FORCE)
    set(WHISPER_NO_AVX2 ON CACHE BOOL "" FORCE)
    set(WHISPER_NO_FMA ON CACHE BOOL "" FORCE)
    set(WHISPER_NO_F16C ON CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)

    # Enable Position Independent Code for Python bindings
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(whisper)

    message(STATUS "whisper.cpp fetched successfully")
    message(STATUS "  Source dir: ${whisper_SOURCE_DIR}")

    # Download Whisper tiny model (39MB)
    set(WHISPER_MODEL_DIR "${CMAKE_BINARY_DIR}/models")
    set(WHISPER_TINY_MODEL "${WHISPER_MODEL_DIR}/ggml-tiny.bin")

    if(NOT EXISTS ${WHISPER_TINY_MODEL})
        message(STATUS "Downloading whisper tiny model (39MB)...")
        file(MAKE_DIRECTORY ${WHISPER_MODEL_DIR})

        file(DOWNLOAD
            "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin"
            ${WHISPER_TINY_MODEL}
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )

        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_CODE)
        if(NOT DOWNLOAD_CODE EQUAL 0)
            message(WARNING "Failed to download whisper model. Please download manually from:")
            message(WARNING "  https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin")
            message(WARNING "  Save to: ${WHISPER_TINY_MODEL}")
        else()
            message(STATUS "Model downloaded successfully")
        endif()
    else()
        message(STATUS "Whisper tiny model already exists at ${WHISPER_TINY_MODEL}")
    endif()

    # Add compile definition for model path
    add_compile_definitions(WHISPER_MODEL_PATH="${WHISPER_TINY_MODEL}")

    list(APPEND FFVOICE_CORE_SOURCES
        src/audio/whisper_processor.cpp
        src/utils/subtitle_generator.cpp
        src/utils/audio_converter.cpp
    )
else()
    message(STATUS "Whisper ASR: Disabled (use -DENABLE_WHISPER=ON to enable)")
endif()

# Python bindings (optional)
if(BUILD_PYTHON)
    message(STATUS "Python bindings: Enabled")

    # Find Python - use PYTHON_EXECUTABLE if provided (from setup.py)
    if(DEFINED PYTHON_EXECUTABLE)
        set(Python_EXECUTABLE ${PYTHON_EXECUTABLE})
        message(STATUS "Using Python from setup.py: ${PYTHON_EXECUTABLE}")
    endif()
    find_package(Python COMPONENTS Interpreter Development REQUIRED)

    # Find pybind11
    find_package(pybind11 CONFIG)

    if(NOT pybind11_FOUND)
        message(STATUS "pybind11 not found in system, fetching from GitHub...")
        include(FetchContent)

        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.11.1
            GIT_SHALLOW TRUE
        )

        FetchContent_MakeAvailable(pybind11)
        message(STATUS "pybind11 fetched successfully")
    else()
        message(STATUS "pybind11 found: ${pybind11_DIR}")
    endif()

    # Python module
    pybind11_add_module(_ffvoice src/python/bindings.cpp)

    target_link_libraries(_ffvoice PRIVATE ffvoice-core)

    target_include_directories(_ffvoice PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    # Install Python module
    install(TARGETS _ffvoice LIBRARY DESTINATION python/ffvoice)
    install(DIRECTORY python/ffvoice/ DESTINATION python/ffvoice
            FILES_MATCHING PATTERN "*.py")
else()
    message(STATUS "Python bindings: Disabled (use -DBUILD_PYTHON=ON to enable)")
endif()

# Core library
add_library(ffvoice-core STATIC ${FFVOICE_CORE_SOURCES})

# Enable Position Independent Code for Python bindings on Linux
set_target_properties(ffvoice-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

if(ENABLE_WHISPER)
    target_link_libraries(ffvoice-core
        PUBLIC whisper
    )
    target_include_directories(ffvoice-core
        PUBLIC ${whisper_SOURCE_DIR}
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
