# FFVoice Engine Test Suite

This directory contains the comprehensive test suite for the FFVoice Engine project, focusing on Milestone 1 audio capture and playback functionality.

## Test Architecture Overview

### Directory Structure

```
tests/
├── CMakeLists.txt              # CMake configuration for tests
├── test_main.cpp               # Google Test main entry point
├── README.md                   # This file
│
├── fixtures/                   # Test fixtures and base classes
│   └── audio_test_fixture.h   # Audio testing fixtures
│
├── mocks/                      # Mock implementations
│   ├── mock_audio_device.h    # Audio device mocks
│   └── mock_file_system.h     # File system mocks
│
├── utils/                      # Test utilities
│   ├── test_signal_generator.h # Audio signal generation
│   └── test_helpers.h         # Common helper functions
│
├── audio/                      # Audio component tests (to be created)
│   ├── test_audio_capture.cpp
│   ├── test_audio_playback.cpp
│   └── test_audio_format.cpp
│
├── processing/                 # Audio processing tests (to be created)
│   ├── test_resampler.cpp
│   ├── test_codec.cpp
│   └── test_vad.cpp
│
├── integration/                # Integration tests (to be created)
│   └── test_audio_pipeline.cpp
│
└── data/                       # Test data files
    └── sample_audio/           # Sample audio files for testing
```

## Components

### 1. Test Fixtures (fixtures/)

**AudioTestFixture** - Base fixture providing:
- Audio configuration management
- Buffer allocation and management
- Signal generation utilities (sine, noise, silence)
- Audio quality metrics (RMS, SNR)
- Buffer comparison with tolerance
- WAV file output for debugging

**Specialized Fixtures**:
- `AudioCaptureTestFixture` - For audio capture tests
- `AudioPlaybackTestFixture` - For audio playback tests
- `AudioProcessingTestFixture` - For processing algorithm tests

### 2. Mock Classes (mocks/)

**Mock Audio Devices**:
- `MockAudioDevice` - Base audio device mock
- `MockAudioCaptureDevice` - Input device mock with data simulation
- `MockAudioPlaybackDevice` - Output device mock with data capture
- `MockAudioDeviceManager` - Device enumeration and creation

**Mock File System**:
- `MockFileReader` - File reading with simulated content
- `MockFileWriter` - File writing with data capture
- `MockFileSystem` - Complete virtual file system

### 3. Test Utilities (utils/)

**TestSignalGenerator** - Generates test signals:
- Basic waveforms (sine, square, triangle, sawtooth)
- Noise (white, pink)
- Chirps and sweeps
- DTMF tones
- Impulses
- Signal mixing and envelope application

**TestHelpers** - Common utilities:
- Floating point comparison with tolerance
- Signal analysis (MSE, correlation, RMS, peak, energy)
- Zero crossing detection
- dB conversion
- WAV header generation
- Performance measurement
- Custom Google Test matchers

## Building and Running Tests

### Build Tests

```bash
# From project root
mkdir -p build
cd build
cmake ..
make

# Build tests specifically
make ffvoice_tests
```

### Run Tests

```bash
# Run all tests
./tests/ffvoice_tests

# Run with verbose output
./tests/ffvoice_tests --verbose

# Run specific test suite
./tests/ffvoice_tests --gtest_filter=AudioCaptureTest.*

# Run specific test case
./tests/ffvoice_tests --gtest_filter=AudioCaptureTest.BasicCapture

# Run with test output
./tests/ffvoice_tests --gtest_output=xml:test_results.xml
```

### Run with CTest

```bash
# Run all tests via CTest
ctest --output-on-failure

# Run with verbose output
ctest -V

# Run specific test
ctest -R AudioCapture

# Run tests in parallel
ctest -j4
```

### Code Coverage

```bash
# Build with coverage
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run tests
./tests/ffvoice_tests

# Generate coverage report
make coverage

# View HTML report
open coverage_html/index.html
```

### Memory Leak Detection (Linux only)

```bash
# Run with Valgrind
make test_memcheck
```

### Performance Benchmarks

```bash
# Run benchmark tests
make benchmark
```

## Writing Tests

### Example: Basic Audio Capture Test

```cpp
#include <gtest/gtest.h>
#include "fixtures/audio_test_fixture.h"
#include "mocks/mock_audio_device.h"
#include "utils/test_signal_generator.h"

using namespace ffvoice::test;

class AudioCaptureTest : public AudioCaptureTestFixture {
protected:
    void SetUp() override {
        AudioCaptureTestFixture::SetUp();
        mock_device_ = std::make_shared<MockAudioCaptureDevice>();
        signal_gen_ = std::make_unique<TestSignalGenerator>(config_.sample_rate);
    }

    std::shared_ptr<MockAudioCaptureDevice> mock_device_;
    std::unique_ptr<TestSignalGenerator> signal_gen_;
};

TEST_F(AudioCaptureTest, CapturesAudioData) {
    // Arrange: Generate test signal
    auto test_signal = signal_gen_->GenerateSineWave(440.0, 1000);
    mock_device_->SimulateCapturedData(test_signal);

    EXPECT_CALL(*mock_device_, Initialize())
        .Times(1)
        .WillOnce(::testing::Return(true));

    // Act: Capture audio
    std::vector<int16_t> captured(1024);
    mock_device_->Initialize();
    size_t frames_read = mock_device_->Read(captured.data(), captured.size());

    // Assert: Verify captured data
    EXPECT_EQ(frames_read, captured.size());
    EXPECT_GT(CalculateRMS(captured), 0.0);
}
```

### Example: Using Mock File System

```cpp
TEST_F(AudioProcessingTest, SavesProcessedAudio) {
    // Arrange
    MockFileSystem mock_fs;
    mock_fs.SetupVirtualFileSystem();

    auto writer = std::make_unique<MockFileWriter>();
    writer->CaptureWrittenData();

    // Act: Process and save audio
    auto test_signal = GenerateSineWave(1000.0, 500);
    // ... processing code ...

    // Assert
    const auto& written_data = writer->GetWrittenData();
    EXPECT_GT(written_data.size(), 0);
}
```

## Test Categories

### Unit Tests
- Individual component testing in isolation
- Use mocks for all dependencies
- Fast execution (< 1ms per test)
- Comprehensive edge case coverage

### Integration Tests
- Multi-component interaction testing
- May use real implementations
- Moderate execution time (< 100ms per test)
- Focus on component interfaces

### Performance Tests
- Benchmark critical operations
- Measure latency and throughput
- Compare against requirements
- Mark with `*Benchmark*` naming convention

## Best Practices

### 1. Test Isolation
- Each test should be independent
- Use fixtures for setup/teardown
- Don't share state between tests
- Clean up resources properly

### 2. Clear Test Names
```cpp
TEST_F(AudioCaptureTest, ReturnsErrorWhenDeviceNotInitialized) { ... }
TEST_F(AudioCaptureTest, HandlesBufferOverflowGracefully) { ... }
```

### 3. AAA Pattern (Arrange-Act-Assert)
```cpp
TEST_F(AudioTest, Example) {
    // Arrange: Set up test conditions
    auto signal = GenerateSineWave(440, 1000);

    // Act: Execute the operation
    auto result = ProcessSignal(signal);

    // Assert: Verify the outcome
    EXPECT_GT(result.size(), 0);
}
```

### 4. Use Appropriate Assertions
- `EXPECT_*` - Test continues on failure
- `ASSERT_*` - Test stops on failure
- Use `ASSERT_*` when subsequent code depends on the assertion

### 5. Test Edge Cases
- Null pointers
- Empty buffers
- Buffer overflows
- Invalid parameters
- Resource exhaustion
- Error conditions

### 6. Mock External Dependencies
- Always mock hardware devices
- Mock file system operations
- Mock network operations
- Use dependency injection

### 7. Measure What Matters
- Code coverage (aim for > 80%)
- Performance metrics
- Memory usage
- Error handling paths

## Continuous Integration

Tests are automatically run on:
- Every commit (via pre-commit hooks)
- Pull requests
- Main branch merges

### CI Configuration
- All tests must pass
- Code coverage must not decrease
- No memory leaks detected
- Performance benchmarks within limits

## Debugging Failed Tests

### View Detailed Output
```bash
./tests/ffvoice_tests --gtest_filter=FailingTest.* --gtest_break_on_failure
```

### Run Under Debugger
```bash
gdb --args ./tests/ffvoice_tests --gtest_filter=FailingTest.*
```

### Generate Debug Audio Files
Tests can write WAV files for inspection:
```cpp
WriteWavFile("debug_output.wav", signal);
```

## Performance Requirements (Milestone 1)

### Audio Capture
- Latency: < 20ms
- CPU usage: < 10% (single core)
- Memory: < 10MB for buffers
- Sample rates: 8kHz, 16kHz, 48kHz
- Formats: 16-bit PCM

### Audio Playback
- Latency: < 20ms
- CPU usage: < 10% (single core)
- No audio dropouts
- Smooth playback

### Code Quality
- Test coverage: > 80%
- No memory leaks
- No resource leaks
- Thread-safe operations

## Contributing

When adding new tests:
1. Follow the existing structure
2. Use appropriate fixtures and mocks
3. Add tests to CMakeLists.txt
4. Document complex test scenarios
5. Ensure tests pass locally before committing
6. Update this README if adding new categories

## Resources

- [Google Test Documentation](https://google.github.io/googletest/)
- [Google Mock Documentation](https://google.github.io/googletest/gmock_for_dummies.html)
- [Audio Testing Best Practices](https://developer.apple.com/documentation/avfoundation/audio_testing)
