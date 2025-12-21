# Test Architecture Quick Start Guide

## Quick Setup

### 1. Build Tests

```bash
cd /Users/haorangong/Github/chicogong/ffvoice-engine
mkdir -p build && cd build
cmake ..
make ffvoice_tests
```

### 2. Run Tests

```bash
# Run all tests
./tests/ffvoice_tests

# Run specific test suite
./tests/ffvoice_tests --gtest_filter=AudioCaptureTest.*

# Run with verbose output
./tests/ffvoice_tests --verbose
```

## Quick Reference: Test Components

### Fixtures (fixtures/audio_test_fixture.h)

```cpp
#include "fixtures/audio_test_fixture.h"

class MyTest : public AudioTestFixture {
protected:
    void SetUp() override {
        AudioTestFixture::SetUp();
        // Your setup
    }
};

TEST_F(MyTest, TestName) {
    // Use fixture methods:
    auto signal = GenerateSineWave(440.0, 1000);
    double rms = CalculateRMS(signal);
    EXPECT_GT(rms, 0.0);
}
```

### Mocks (mocks/mock_audio_device.h)

```cpp
#include "mocks/mock_audio_device.h"

TEST(MyTest, MockExample) {
    auto mock = std::make_shared<MockAudioCaptureDevice>();

    // Set expectations
    EXPECT_CALL(*mock, Initialize()).WillOnce(Return(true));

    // Simulate captured data
    auto signal = GenerateSineWave(440.0, 1000);
    mock->SimulateCapturedData(signal);

    // Test your code
    bool result = mock->Initialize();
    EXPECT_TRUE(result);
}
```

### Signal Generator (utils/test_signal_generator.h)

```cpp
#include "utils/test_signal_generator.h"

TestSignalGenerator gen(16000);  // 16kHz

// Basic waveforms
auto sine = gen.GenerateSineWave(440.0, 1000, 0.5);
auto square = gen.GenerateSquareWave(440.0, 1000);
auto triangle = gen.GenerateTriangleWave(440.0, 1000);

// Noise
auto white = gen.GenerateWhiteNoise(1000, 0.1);
auto pink = gen.GeneratePinkNoise(1000, 0.1);

// Special signals
auto chirp = gen.GenerateChirp(100.0, 1000.0, 1000);
auto dtmf = gen.GenerateDTMF('5', 200);
auto impulse = gen.GenerateImpulse(100);
```

### Test Helpers (utils/test_helpers.h)

```cpp
#include "utils/test_helpers.h"

// Signal analysis
double rms = TestHelpers::CalculateRMS(signal);
int16_t peak = TestHelpers::CalculatePeak(signal);
double energy = TestHelpers::CalculateEnergy(signal);

// Signal comparison
double mse = TestHelpers::CalculateMSE(signal1, signal2);
double corr = TestHelpers::CalculateCorrelation(signal1, signal2);

// Floating point comparison
EXPECT_TRUE(TestHelpers::ApproximatelyEqual(a, b, 1e-6));

// dB conversion
double db = TestHelpers::AmplitudeToDecibels(amplitude);
double amp = TestHelpers::DecibelsToAmplitude(db);

// Performance measurement
double ms = TestHelpers::MeasureExecutionTime([&]() {
    // Code to measure
});
```

## Common Test Patterns

### Pattern 1: Basic Unit Test

```cpp
TEST(ComponentTest, BasicFunctionality) {
    // Arrange
    auto component = CreateComponent();

    // Act
    auto result = component.DoSomething();

    // Assert
    EXPECT_TRUE(result.IsValid());
}
```

### Pattern 2: Test with Fixture

```cpp
class MyComponentTest : public AudioTestFixture {
protected:
    std::unique_ptr<MyComponent> component_;

    void SetUp() override {
        AudioTestFixture::SetUp();
        component_ = std::make_unique<MyComponent>();
    }
};

TEST_F(MyComponentTest, TestCase) {
    auto signal = GenerateSineWave(440.0, 1000);
    auto result = component_->Process(signal);
    EXPECT_GT(CalculateRMS(result), 0.0);
}
```

### Pattern 3: Test with Mocks

```cpp
TEST(AudioTest, WithMockDevice) {
    auto mock_device = std::make_shared<MockAudioCaptureDevice>();

    // Set expectations
    EXPECT_CALL(*mock_device, Initialize())
        .Times(1)
        .WillOnce(Return(true));

    // Simulate data
    TestSignalGenerator gen(16000);
    mock_device->SimulateCapturedData(gen.GenerateSineWave(440, 1000));

    // Test
    auto component = CreateComponent(mock_device);
    EXPECT_TRUE(component.Start());
}
```

### Pattern 4: Performance Test

```cpp
TEST(PerformanceTest, BenchmarkProcessing) {
    TestSignalGenerator gen(16000);
    auto signal = gen.GenerateSineWave(440, 10000);  // 10 seconds

    double elapsed = TestHelpers::MeasureExecutionTime([&]() {
        ProcessAudio(signal);
    });

    EXPECT_LT(elapsed, 100.0);  // Should complete in < 100ms
    std::cout << "Processing took " << elapsed << " ms\n";
}
```

### Pattern 5: Signal Quality Test

```cpp
TEST(QualityTest, MaintainsSignalQuality) {
    TestSignalGenerator gen(16000);
    auto original = gen.GenerateSineWave(1000, 1000);

    auto processed = ProcessAudio(original);

    // Check correlation (should be high)
    double corr = TestHelpers::CalculateCorrelation(original, processed);
    EXPECT_GT(corr, 0.95);

    // Check MSE (should be low)
    double mse = TestHelpers::CalculateMSE(original, processed);
    EXPECT_LT(mse, 100.0);
}
```

## File Organization

Add your tests to appropriate directories:

```
tests/
├── audio/                  # Audio I/O tests
│   ├── test_audio_capture.cpp
│   └── test_audio_playback.cpp
├── processing/             # Processing algorithm tests
│   ├── test_resampler.cpp
│   └── test_codec.cpp
└── integration/            # Integration tests
    └── test_audio_pipeline.cpp
```

## CMake Integration

Add your test files to `tests/CMakeLists.txt`:

```cmake
set(TEST_SOURCES
    test_main.cpp
    audio/test_audio_capture.cpp
    audio/test_audio_playback.cpp
    # Add your tests here
)
```

## Best Practices

1. **One assertion per logical concept**
   ```cpp
   TEST(Test, MultipleAssertions) {
       EXPECT_EQ(result.status, Status::OK);
       EXPECT_GT(result.data.size(), 0);
       EXPECT_TRUE(result.IsValid());
   }
   ```

2. **Use descriptive test names**
   ```cpp
   TEST(AudioCapture, ReturnsErrorWhenDeviceNotInitialized)
   TEST(AudioCapture, CapturesMonoAudioAt16kHz)
   ```

3. **Test edge cases**
   ```cpp
   TEST(AudioBuffer, HandlesEmptyBuffer)
   TEST(AudioBuffer, HandlesMaxSizeBuffer)
   TEST(AudioBuffer, HandleNullPointer)
   ```

4. **Use appropriate matchers**
   ```cpp
   EXPECT_THAT(values, ::testing::ElementsAre(1, 2, 3));
   EXPECT_THAT(text, ::testing::HasSubstr("error"));
   EXPECT_THAT(value, ::testing::AllOf(Ge(0), Le(100)));
   ```

## Debugging Tests

### Run specific test with debug info

```bash
./tests/ffvoice_tests --gtest_filter=MyTest.* --gtest_break_on_failure
```

### Run under GDB

```bash
gdb --args ./tests/ffvoice_tests --gtest_filter=MyTest.*
(gdb) run
```

### Enable verbose output

```bash
./tests/ffvoice_tests --verbose --gtest_filter=MyTest.*
```

## Common Issues

### Issue: Test times out
**Solution**: Check for infinite loops or blocking operations

### Issue: Flaky test (intermittent failures)
**Solution**: Look for race conditions, timing dependencies, or uninitialized variables

### Issue: Mock expectations not met
**Solution**: Verify EXPECT_CALL is set before the method is called

### Issue: Compilation errors
**Solution**: Ensure all headers are included and CMakeLists.txt is updated

## Resources

- Full documentation: `tests/README.md`
- Example tests: `tests/audio/test_audio_capture_example.cpp`
- Architecture examples: `tests/utils/test_architecture_example.cpp`
- Google Test docs: https://google.github.io/googletest/
- Google Mock docs: https://google.github.io/googletest/gmock_for_dummies.html
