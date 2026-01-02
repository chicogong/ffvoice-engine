# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ffvoice-engine is a high-performance C++ audio processing engine (v0.6.0) for real-time audio capture, AI-powered enhancement, and offline speech recognition. It's designed as a production-ready library with Python bindings, targeting 100% offline operation with 3-10x better performance than pure Python solutions.

**Core capabilities**: Real-time audio I/O (PortAudio), AI noise reduction (RNNoise), speech recognition (Whisper), lossless compression (FLAC), and intelligent VAD segmentation.

## Common Commands

### Build System

```bash
# Standard build (minimal features)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Full-featured build (recommended for development)
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_RNNOISE=ON \
  -DENABLE_WHISPER=ON \
  -DBUILD_TESTS=ON
make -j$(nproc)

# Python package build
pip install .  # Uses setup.py with custom CMakeBuild
```

### Testing

```bash
# Build with tests enabled
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make -j4

# Run all tests
make test  # or: ctest

# Run tests with verbose output
make test_verbose

# Run specific test suite
./build/tests/ffvoice_tests --gtest_filter=WavWriter*
./build/tests/ffvoice_tests --gtest_filter=RNNoiseProcessor*

# Code coverage (Linux only, Debug build)
make coverage  # Generates coverage_html/

# Memory leak detection (Linux with Valgrind)
make test_memcheck
```

### Code Quality

```bash
# Format code (runs clang-format)
./scripts/format.sh

# Run linting (runs clang-tidy)
./scripts/lint.sh
```

### Development Workflow

```bash
# CLI usage examples
./build/ffvoice --list-devices
./build/ffvoice --record -o test.wav -t 10
./build/ffvoice --record -o test.flac --rnnoise --normalize -t 30
./build/ffvoice --transcribe audio.wav --format srt -o output.srt

# Python development (from repo root)
pip install -e .  # Editable install for development
python python/examples/basic_transcribe.py
```

## Architecture Overview

### Processing Pipeline Architecture

The engine uses a **Chain of Responsibility** pattern for audio processing:

```
AudioCaptureDevice (PortAudio callback thread)
    ↓ (int16_t samples, real-time constraints)
AudioProcessorChain (modular, zero-copy)
    ├→ HighPassFilter (IIR, 80Hz cutoff)
    ├→ RNNoiseProcessor (AI denoise + VAD)
    └→ VolumeNormalizer (RMS-based AGC)
    ↓ (processed samples)
WavWriter / FlacWriter
    ↓
Disk Storage
```

**Real-time transcription pipeline**:
```
AudioCapture → RNNoiseProcessor (VAD) → VADSegmenter → WhisperProcessor → Subtitles
```

### Core Components

**Audio I/O Layer** (`src/audio/audio_capture_device.*`):
- Wraps PortAudio for cross-platform capture
- Thread-safe callback mechanism with atomic lifecycle flags
- Device enumeration and selection

**Processing Layer** (`src/audio/audio_processor.*`):
- Abstract `AudioProcessor` interface for extensibility
- `AudioProcessorChain` for chaining multiple processors
- In-place processing (zero-copy) for real-time performance
- Implementations: `VolumeNormalizer`, `HighPassFilter`, `RNNoiseProcessor`

**AI/ML Layer**:
- `RNNoiseProcessor` (`src/audio/rnnoise_processor.*`): Deep learning noise suppression with frame rebuffering (256→480 samples)
- `WhisperProcessor` (`src/audio/whisper_processor.*`): Offline ASR with automatic audio conversion
- `VADSegmenter` (`src/audio/vad_segmenter.*`): State machine for intelligent speech segmentation

**Media I/O Layer**:
- `WavWriter` (`src/media/wav_writer.*`): Hand-written RIFF/WAV format (no external deps)
- `FlacWriter` (`src/media/flac_writer.*`): libFLAC integration, 2-3x compression
- `AudioConverter` (`src/utils/audio_converter.*`): Format/sample-rate conversion for Whisper

**Python Bindings** (`src/python/bindings.cpp`):
- pybind11 with NumPy integration (zero-copy buffer sharing)
- Mirrors C++ API with Pythonic exceptions

### Key Design Patterns

1. **Zero-Copy Processing**: All processors use in-place `Process(int16_t* samples, size_t num_samples)` to avoid allocations
2. **Frame Rebuffering**: RNNoise requires 480-sample frames; accumulator buffer handles PortAudio's 256-sample callbacks
3. **Reusable Buffers**: WhisperProcessor reuses conversion/resample buffers (90% allocation reduction)
4. **Thread-Safe Lifecycle**: Atomic `callback_active_` flag prevents race conditions during stop
5. **Optional Feature Isolation**: `#ifdef ENABLE_WHISPER` for minimal binary size and clean dependencies

### Critical Code Paths

**Audio Capture Flow** (real-time, <100ms latency):
1. `AudioCaptureDevice::Start(callback)` → PortAudio thread
2. `PortAudioCallback()` checks `callback_active_` atomic flag
3. User callback processes through `AudioProcessorChain`
4. Write to `WavWriter`/`FlacWriter` via buffered I/O

**Offline Transcription** (`WhisperProcessor::TranscribeFile`):
1. `AudioConverter::LoadAudioFile()` → decode WAV/FLAC (FFmpeg)
2. Resample 48kHz→16kHz, convert int16→float, stereo→mono
3. `whisper_full()` inference (whisper.cpp)
4. `ExtractSegments()` → `SubtitleGenerator` (SRT/VTT/TXT)

**Real-time Transcription** (VAD-triggered):
1. `RNNoiseProcessor::Process()` outputs VAD probability (0.0-1.0)
2. `VADSegmenter::ProcessFrame()` state machine detects speech boundaries
3. Callback triggered with complete segment buffer
4. `WhisperProcessor::TranscribeBuffer()` processes segment asynchronously

## Important Implementation Details

### RNNoise Frame Size Handling
- RNNoise requires exactly 480 samples per frame (10ms @ 48kHz)
- PortAudio typically uses 256-sample buffers
- `RNNoiseProcessor` maintains a `rebuffer_` accumulator to handle this mismatch
- When modifying: ensure frame alignment or denoise quality degrades

### Whisper Audio Format Requirements
- Whisper expects: 16kHz sample rate, float32 format, mono channel
- Input audio is typically: 48kHz, int16, stereo
- `AudioConverter` handles all conversions automatically
- Use `conversion_buffer_` and `resample_buffer_` for performance (reused across calls)

### VAD Segmenter State Machine
- States: `SILENCE` → `SPEECH` → `SILENCE` (triggers segment)
- Configurable sensitivity presets (5 levels): `VERY_SENSITIVE` to `VERY_CONSERVATIVE`
- Adaptive threshold dynamically adjusts to environment noise
- Min speech duration and silence duration prevent false triggers

### Memory Optimization Strategy
- Avoid allocations in audio callback (real-time constraint)
- Pre-allocate buffers in `Initialize()`, reuse in `Process()`
- Use `reserve()` instead of `resize()` when size is known
- Conditional expansion: only grow buffers when necessary

### Platform-Specific Notes
- **macOS**: Native ARM64 support, deployment target 11.0
- **Linux**: System packages via apt/yum, Valgrind support
- **Windows**: vcpkg for deps, RNNoise disabled (MSVC VLA incompatibility)
- Apple Silicon: Use native ARM64 Python, not Rosetta

## CMake Build Configuration

### Key Options
- `BUILD_TESTS=ON/OFF` - Build Google Test suite (default: ON)
- `BUILD_EXAMPLES=ON/OFF` - Build example apps (default: ON)
- `BUILD_PYTHON=ON/OFF` - Build Python bindings (default: OFF for C++ build)
- `ENABLE_RNNOISE=ON/OFF` - Auto-download RNNoise (not Windows/MSVC)
- `ENABLE_WHISPER=ON/OFF` - Auto-download whisper.cpp + tiny model
- `ENABLE_WEBRTC_APM=ON/OFF` - Requires manual WebRTC APM install

### Dependency Management
- FFmpeg/PortAudio/FLAC: System packages (brew/apt/vcpkg)
- whisper.cpp: CMake FetchContent auto-download (v1.5.4)
- RNNoise: FetchContent from Xiph repo
- Google Test: FetchContent (v1.14.0)
- pybind11: FetchContent (v2.11.1)

## File Organization Patterns

### Adding New Audio Processors
1. Create header in `include/ffvoice/` or `src/audio/` depending on visibility
2. Inherit from `AudioProcessor` interface (see `src/audio/audio_processor.h`)
3. Implement `Initialize()`, `Process()`, `Reset()` methods
4. Add to `CMakeLists.txt` under `FFVOICE_SOURCES`
5. Write unit tests in `tests/unit/test_<name>.cpp`
6. Add to `AudioProcessorChain` if needed for CLI

### Python Binding Integration
1. Include C++ header in `src/python/bindings.cpp`
2. Add pybind11 class definition in `PYBIND11_MODULE` block
3. Expose methods with `.def()`, handle NumPy arrays with `py::array_t<int16_t>`
4. Add examples to `python/examples/`
5. Update `python/README.md` with usage

## Testing Strategy

### Test Structure
- **Unit tests** (`tests/unit/`): Component-level, 39+ tests covering core modules
- **Mocks** (`tests/mocks/`): Mock implementations for audio devices and file I/O
- **Fixtures** (`tests/fixtures/`): Test data generators (SignalGenerator for deterministic audio)

### Coverage Targets
- WavWriter: 16 tests (format compliance, edge cases)
- SignalGenerator: 23 tests (signal accuracy, boundary conditions)
- FlacWriter, AudioConverter, VADSegmenter, RNNoise, Logger: Full coverage

### Debugging Audio Issues
1. Enable debug logging: `Logger::SetLogLevel(LogLevel::DEBUG)`
2. Use `SignalGenerator` for reproducible test signals (sine waves, silence, noise)
3. Check sample rate mismatches (48kHz input vs 16kHz Whisper requirement)
4. Verify buffer alignment with frame requirements (480 for RNNoise)
5. Inspect output files with `ffplay`, `audacity`, or `ffprobe`

## Performance Characteristics

### Benchmarks (Apple M3 Pro, Rosetta 2)
- AudioCapture latency: <100ms (PortAudio)
- RNNoise: ~8% CPU, real-time processing, ~5MB per channel state
- Whisper TINY: 5-75x realtime (depends on audio length), ~272MB memory
- Whisper BASE: ~7x realtime, ~350MB memory
- FLAC compression: Real-time capable, 2-3x compression ratio

### Optimization Opportunities
- Buffer reuse in WhisperProcessor reduces allocations by 90%
- Conditional buffer expansion avoids unnecessary resizes
- RAII ensures automatic cleanup (no manual memory management)
- Native CPU optimizations: `-march=native` on x86_64

## Logging System

Use the unified logging system (`utils/logger.h`):
```cpp
#include "utils/logger.h"

LOG_ERROR("Critical error: {}", error_msg);
LOG_WARNING("Non-fatal issue: {}", warning);
LOG_INFO("Status update: {}", status);
LOG_DEBUG("Detailed trace: value={}", value);
```

Thread-safe, color-coded output, configurable log levels.

## Current Limitations

- RNNoise disabled on Windows (MSVC doesn't support C99 VLA)
- WebRTC APM requires manual installation (not auto-downloaded)
- Intel Mac users must build from source (no PyPI wheels)
- Whisper inference is CPU-only (no GPU acceleration yet)

## Version Information

- Current version: 0.6.0 (production ready)
- C++ standard: C++20 (required)
- Python support: 3.9-3.12
- Platform support: macOS ARM64, Linux x86_64, Windows x86_64 (partial)