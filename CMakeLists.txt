# ffvoice-engine - Low-latency C++ voice engine
cmake_minimum_required(VERSION 3.20)
project(ffvoice-engine VERSION 0.7.0 LANGUAGES CXX C)

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
option(ENABLE_RNNOISE "Enable RNNoise deep learning noise suppression" OFF)
option(ENABLE_WHISPER "Enable Whisper ASR (speech recognition)" OFF)
option(ENABLE_DIARIZATION "Enable speaker diarization (sherpa-onnx + ONNX Runtime)" OFF)

# Pinned ONNX Runtime version for speaker diarization (prebuilt, not built from source)
set(ORT_VERSION "1.20.1" CACHE STRING "ONNX Runtime version used for speaker diarization")

# Propagate the project version to the C++ sources (single source of truth)
add_compile_definitions(FFVOICE_VERSION="${PROJECT_VERSION}")

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

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${FLAC_INCLUDE_DIRS}
)

# Source files
set(FFVOICE_CORE_SOURCES
    src/audio/audio_capture_device.cpp
    src/audio/audio_mixer.cpp
    src/audio/audio_processor.cpp
    src/audio/diarizer.cpp
    src/audio/vad_segmenter.cpp
    src/media/wav_writer.cpp
    src/media/flac_writer.cpp
    src/utils/logger.cpp
    src/utils/ring_buffer.cpp
    src/utils/signal_generator.cpp
    src/utils/word_grouper.cpp
)

# RNNoise (conditional - not supported on Windows due to VLA)
if(ENABLE_RNNOISE)
    if(MSVC)
        message(WARNING "RNNoise is not supported on Windows (MSVC doesn't support C99 VLA)")
        message(STATUS "RNNoise: Disabled on Windows")
        set(ENABLE_RNNOISE OFF CACHE BOOL "" FORCE)
    else()
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
    endif()  # End of if(MSVC) else block
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
        src/audio/live_captioner.cpp
        src/utils/subtitle_generator.cpp
        src/utils/audio_converter.cpp
    )
else()
    message(STATUS "Whisper ASR: Disabled (use -DENABLE_WHISPER=ON to enable)")
endif()

# Speaker Diarization (conditional - sherpa-onnx + prebuilt ONNX Runtime)
# Not supported on Windows: sherpa-onnx links a static ONNX Runtime that
# conflicts with the MSVC C runtime. Diarization is Windows-unsupported for now.
if(ENABLE_DIARIZATION)
    if(MSVC)
        message(WARNING "Speaker diarization is not supported on Windows (sherpa-onnx's static ONNX Runtime conflicts with the MSVC CRT)")
        message(STATUS "Speaker Diarization: Disabled on Windows")
        set(ENABLE_DIARIZATION OFF CACHE BOOL "" FORCE)
    elseif(NOT ENABLE_WHISPER)
        message(FATAL_ERROR "ENABLE_DIARIZATION requires ENABLE_WHISPER=ON (shared ASR/ggml pipeline). Re-run cmake with -DENABLE_WHISPER=ON.")
    else()
        message(STATUS "Speaker Diarization: Enabled")

        include(FetchContent)

        # --- 1. Prebuilt ONNX Runtime (downloaded, NOT built from source) -----
        # Select the platform tarball from the official ONNX Runtime release.
        if(APPLE)
            if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR
               (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64"))
                set(ORT_PLATFORM "osx-arm64")
            else()
                set(ORT_PLATFORM "osx-x86_64")
            endif()
        elseif(UNIX)
            set(ORT_PLATFORM "linux-x64")
        else()
            message(FATAL_ERROR "Speaker diarization: unsupported platform for prebuilt ONNX Runtime")
        endif()

        set(ORT_ARCHIVE "onnxruntime-${ORT_PLATFORM}-${ORT_VERSION}.tgz")
        set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/${ORT_ARCHIVE}")

        message(STATUS "Fetching prebuilt ONNX Runtime ${ORT_VERSION} (${ORT_PLATFORM})...")
        message(STATUS "  URL: ${ORT_URL}")

        FetchContent_Declare(
            onnxruntime_prebuilt
            URL ${ORT_URL}
        )
        FetchContent_MakeAvailable(onnxruntime_prebuilt)

        # The tarball extracts to <root>/{include,lib}. Point sherpa-onnx at it.
        # sherpa-onnx reads these as environment variables (see its
        # cmake/onnxruntime.cmake) to skip its own ORT download.
        set(SHERPA_ONNXRUNTIME_INCLUDE_DIR "${onnxruntime_prebuilt_SOURCE_DIR}/include"
            CACHE PATH "ONNX Runtime headers for sherpa-onnx" FORCE)
        set(SHERPA_ONNXRUNTIME_LIB_DIR "${onnxruntime_prebuilt_SOURCE_DIR}/lib"
            CACHE PATH "ONNX Runtime libraries for sherpa-onnx" FORCE)
        set(ENV{SHERPA_ONNXRUNTIME_INCLUDE_DIR} "${SHERPA_ONNXRUNTIME_INCLUDE_DIR}")
        set(ENV{SHERPA_ONNXRUNTIME_LIB_DIR} "${SHERPA_ONNXRUNTIME_LIB_DIR}")

        message(STATUS "  ONNX Runtime include dir: ${SHERPA_ONNXRUNTIME_INCLUDE_DIR}")
        message(STATUS "  ONNX Runtime lib dir: ${SHERPA_ONNXRUNTIME_LIB_DIR}")

        # --- 2. sherpa-onnx (speaker-diarization parts only) ------------------
        # Restrict the build to the speaker-diarization code path: no binaries,
        # no websocket server, no TTS, no python, no portaudio.
        set(SHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION ON  CACHE BOOL "" FORCE)
        set(SHERPA_ONNX_ENABLE_BINARY             OFF CACHE BOOL "" FORCE)
        set(SHERPA_ONNX_ENABLE_WEBSOCKET          OFF CACHE BOOL "" FORCE)
        set(SHERPA_ONNX_ENABLE_TTS                OFF CACHE BOOL "" FORCE)
        set(SHERPA_ONNX_ENABLE_PYTHON             OFF CACHE BOOL "" FORCE)
        set(SHERPA_ONNX_ENABLE_PORTAUDIO          OFF CACHE BOOL "" FORCE)
        set(SHERPA_ONNX_ENABLE_TESTS              OFF CACHE BOOL "" FORCE)
        # C API headers/library so a later PR can #include "sherpa-onnx/c-api/c-api.h".
        set(SHERPA_ONNX_ENABLE_C_API              ON  CACHE BOOL "" FORCE)
        set(SHERPA_ONNX_BUILD_C_API_EXAMPLES      OFF CACHE BOOL "" FORCE)
        # Use the prebuilt ONNX Runtime fetched above instead of letting
        # sherpa-onnx download its own copy.
        set(SHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE ON CACHE BOOL "" FORCE)

        message(STATUS "Fetching sherpa-onnx from GitHub...")

        # sherpa-onnx forces C++20 globally, but one of its transitive,
        # unconditionally-included deps (simple-sentencepiece v0.7) still uses
        # std::result_of, which libc++ removed in C++20. Re-enable the removed
        # type traits for the dependency build via the documented libc++ escape
        # hatch. Applied here (before FetchContent_MakeAvailable) so it reaches
        # sherpa-onnx's sub-build targets; only affects the diarization build.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_definitions(_LIBCPP_ENABLE_CXX20_REMOVED_TYPE_TRAITS)
        endif()

        FetchContent_Declare(
            sherpa_onnx
            GIT_REPOSITORY https://github.com/k2-fsa/sherpa-onnx.git
            GIT_TAG v1.12.40
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(sherpa_onnx)

        message(STATUS "sherpa-onnx fetched successfully")
        message(STATUS "  Source dir: ${sherpa_onnx_SOURCE_DIR}")

        # --- 3. Diarization model files ---------------------------------------
        # Downloaded (only if absent) from the sherpa-onnx release tags
        # "speaker-segmentation-models" and "speaker-recongition-models".
        # NOTE: the embedding-models tag is literally spelled "recongition"
        # (a known upstream typo) - do not "fix" it.
        set(DIARIZATION_MODEL_DIR "${CMAKE_BINARY_DIR}/models/diarization")
        file(MAKE_DIRECTORY ${DIARIZATION_MODEL_DIR})

        # Pyannote segmentation model (~6MB, distributed as a .tar.bz2 archive).
        set(DIARIZATION_SEG_ARCHIVE "${DIARIZATION_MODEL_DIR}/sherpa-onnx-pyannote-segmentation-3-0.tar.bz2")
        set(DIARIZATION_SEG_DIR "${DIARIZATION_MODEL_DIR}/sherpa-onnx-pyannote-segmentation-3-0")
        set(DIARIZATION_SEGMENTATION_MODEL "${DIARIZATION_SEG_DIR}/model.onnx")
        set(DIARIZATION_SEG_URL "https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-segmentation-models/sherpa-onnx-pyannote-segmentation-3-0.tar.bz2")

        if(NOT EXISTS ${DIARIZATION_SEGMENTATION_MODEL})
            message(STATUS "Downloading pyannote segmentation model...")
            file(DOWNLOAD
                "${DIARIZATION_SEG_URL}"
                "${DIARIZATION_SEG_ARCHIVE}"
                SHOW_PROGRESS
                STATUS DIARIZATION_SEG_STATUS
            )
            list(GET DIARIZATION_SEG_STATUS 0 DIARIZATION_SEG_CODE)
            if(NOT DIARIZATION_SEG_CODE EQUAL 0)
                message(FATAL_ERROR "Failed to download diarization segmentation model from ${DIARIZATION_SEG_URL}")
            endif()
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xjf "${DIARIZATION_SEG_ARCHIVE}"
                WORKING_DIRECTORY "${DIARIZATION_MODEL_DIR}"
                RESULT_VARIABLE DIARIZATION_SEG_EXTRACT
            )
            if(NOT DIARIZATION_SEG_EXTRACT EQUAL 0)
                message(FATAL_ERROR "Failed to extract ${DIARIZATION_SEG_ARCHIVE}")
            endif()
            message(STATUS "Segmentation model ready: ${DIARIZATION_SEGMENTATION_MODEL}")
        else()
            message(STATUS "Segmentation model already exists at ${DIARIZATION_SEGMENTATION_MODEL}")
        endif()

        # Speaker-embedding model (a single .onnx file - no archive).
        set(DIARIZATION_EMBEDDING_MODEL "${DIARIZATION_MODEL_DIR}/3dspeaker_speech_campplus_sv_en_voxceleb_16k.onnx")
        set(DIARIZATION_EMB_URL "https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-recongition-models/3dspeaker_speech_campplus_sv_en_voxceleb_16k.onnx")

        if(NOT EXISTS ${DIARIZATION_EMBEDDING_MODEL})
            message(STATUS "Downloading speaker embedding model...")
            file(DOWNLOAD
                "${DIARIZATION_EMB_URL}"
                "${DIARIZATION_EMBEDDING_MODEL}"
                SHOW_PROGRESS
                STATUS DIARIZATION_EMB_STATUS
            )
            list(GET DIARIZATION_EMB_STATUS 0 DIARIZATION_EMB_CODE)
            if(NOT DIARIZATION_EMB_CODE EQUAL 0)
                message(FATAL_ERROR "Failed to download diarization embedding model from ${DIARIZATION_EMB_URL}")
            endif()
            message(STATUS "Embedding model ready: ${DIARIZATION_EMBEDDING_MODEL}")
        else()
            message(STATUS "Embedding model already exists at ${DIARIZATION_EMBEDDING_MODEL}")
        endif()

        # --- 4. Compile definitions for the (future) Diarizer class -----------
        add_compile_definitions(
            ENABLE_DIARIZATION
            DIARIZATION_SEGMENTATION_MODEL_PATH="${DIARIZATION_SEGMENTATION_MODEL}"
            DIARIZATION_EMBEDDING_MODEL_PATH="${DIARIZATION_EMBEDDING_MODEL}"
        )
    endif()  # End of if(MSVC)/elseif(NOT ENABLE_WHISPER)/else block
else()
    message(STATUS "Speaker Diarization: Disabled (use -DENABLE_DIARIZATION=ON to enable)")
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
            GIT_TAG v2.13.6
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

if(ENABLE_DIARIZATION)
    # sherpa-onnx C API target. It transitively brings in sherpa-onnx-core and
    # the prebuilt ONNX Runtime, and exposes the "sherpa-onnx/c-api/c-api.h"
    # include path so a later PR can write the Diarizer class.
    target_link_libraries(ffvoice-core
        PUBLIC sherpa-onnx-c-api
    )
    target_include_directories(ffvoice-core
        PUBLIC ${sherpa_onnx_SOURCE_DIR}
    )
    target_link_directories(ffvoice-core
        PUBLIC ${SHERPA_ONNXRUNTIME_LIB_DIR}
    )
endif()

target_link_directories(ffvoice-core
    PUBLIC
        ${FFMPEG_LIBRARY_DIRS}
        ${PORTAUDIO_LIBRARY_DIRS}
        ${FLAC_LIBRARY_DIRS}
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
