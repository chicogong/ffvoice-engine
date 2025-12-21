/**
 * @file test_helpers.h
 * @brief Common test helper functions and utilities
 *
 * Provides utility functions for test data generation, comparison,
 * validation, and other common testing operations.
 */

#ifndef FFVOICE_TESTS_UTILS_TEST_HELPERS_H
#define FFVOICE_TESTS_UTILS_TEST_HELPERS_H

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace ffvoice {
namespace test {

/**
 * @class TestHelpers
 * @brief Collection of test helper functions
 */
class TestHelpers {
public:
    /**
     * @brief Compare floating point values with tolerance
     *
     * @param a First value
     * @param b Second value
     * @param epsilon Tolerance (default 1e-6)
     * @return True if values are approximately equal
     */
    static bool ApproximatelyEqual(double a, double b, double epsilon = 1e-6) {
        return std::abs(a - b) < epsilon;
    }

    /**
     * @brief Compare floating point vectors with tolerance
     *
     * @param a First vector
     * @param b Second vector
     * @param epsilon Tolerance per element
     * @return True if vectors are approximately equal
     */
    static bool VectorsApproximatelyEqual(
        const std::vector<double>& a,
        const std::vector<double>& b,
        double epsilon = 1e-6
    ) {
        if (a.size() != b.size()) return false;

        for (size_t i = 0; i < a.size(); ++i) {
            if (!ApproximatelyEqual(a[i], b[i], epsilon)) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Calculate Mean Squared Error between two signals
     *
     * @param signal1 First signal
     * @param signal2 Second signal
     * @return MSE value
     */
    static double CalculateMSE(
        const std::vector<int16_t>& signal1,
        const std::vector<int16_t>& signal2
    ) {
        if (signal1.size() != signal2.size() || signal1.empty()) {
            return INFINITY;
        }

        double sum = 0.0;
        for (size_t i = 0; i < signal1.size(); ++i) {
            double diff = signal1[i] - signal2[i];
            sum += diff * diff;
        }

        return sum / signal1.size();
    }

    /**
     * @brief Calculate correlation coefficient between two signals
     *
     * @param signal1 First signal
     * @param signal2 Second signal
     * @return Correlation coefficient (-1.0 to 1.0)
     */
    static double CalculateCorrelation(
        const std::vector<int16_t>& signal1,
        const std::vector<int16_t>& signal2
    ) {
        if (signal1.size() != signal2.size() || signal1.empty()) {
            return 0.0;
        }

        // Calculate means
        double mean1 = 0.0, mean2 = 0.0;
        for (size_t i = 0; i < signal1.size(); ++i) {
            mean1 += signal1[i];
            mean2 += signal2[i];
        }
        mean1 /= signal1.size();
        mean2 /= signal2.size();

        // Calculate correlation
        double numerator = 0.0;
        double sum_sq1 = 0.0, sum_sq2 = 0.0;

        for (size_t i = 0; i < signal1.size(); ++i) {
            double diff1 = signal1[i] - mean1;
            double diff2 = signal2[i] - mean2;

            numerator += diff1 * diff2;
            sum_sq1 += diff1 * diff1;
            sum_sq2 += diff2 * diff2;
        }

        double denominator = std::sqrt(sum_sq1 * sum_sq2);
        return (denominator != 0.0) ? (numerator / denominator) : 0.0;
    }

    /**
     * @brief Calculate RMS (Root Mean Square) of signal
     *
     * @param signal Input signal
     * @return RMS value
     */
    static double CalculateRMS(const std::vector<int16_t>& signal) {
        if (signal.empty()) return 0.0;

        double sum = 0.0;
        for (int16_t sample : signal) {
            sum += static_cast<double>(sample) * sample;
        }

        return std::sqrt(sum / signal.size());
    }

    /**
     * @brief Calculate peak amplitude of signal
     *
     * @param signal Input signal
     * @return Peak amplitude (absolute value)
     */
    static int16_t CalculatePeak(const std::vector<int16_t>& signal) {
        if (signal.empty()) return 0;

        int16_t peak = 0;
        for (int16_t sample : signal) {
            int16_t abs_sample = std::abs(sample);
            if (abs_sample > peak) {
                peak = abs_sample;
            }
        }

        return peak;
    }

    /**
     * @brief Detect zero crossings in signal
     *
     * @param signal Input signal
     * @return Number of zero crossings
     */
    static size_t CountZeroCrossings(const std::vector<int16_t>& signal) {
        if (signal.size() < 2) return 0;

        size_t count = 0;
        for (size_t i = 1; i < signal.size(); ++i) {
            if ((signal[i-1] < 0 && signal[i] >= 0) ||
                (signal[i-1] >= 0 && signal[i] < 0)) {
                ++count;
            }
        }

        return count;
    }

    /**
     * @brief Calculate signal energy
     *
     * @param signal Input signal
     * @return Signal energy
     */
    static double CalculateEnergy(const std::vector<int16_t>& signal) {
        double energy = 0.0;
        for (int16_t sample : signal) {
            energy += static_cast<double>(sample) * sample;
        }
        return energy;
    }

    /**
     * @brief Normalize signal to range [-1.0, 1.0]
     *
     * @param signal Input signal
     * @return Normalized signal (as doubles)
     */
    static std::vector<double> NormalizeSignal(const std::vector<int16_t>& signal) {
        std::vector<double> normalized(signal.size());

        for (size_t i = 0; i < signal.size(); ++i) {
            normalized[i] = signal[i] / 32768.0;
        }

        return normalized;
    }

    /**
     * @brief Convert samples to decibels (dB)
     *
     * @param amplitude Amplitude value
     * @param reference Reference value (default 32768.0 for int16_t)
     * @return dB value
     */
    static double AmplitudeToDecibels(double amplitude, double reference = 32768.0) {
        if (amplitude <= 0.0) return -INFINITY;
        return 20.0 * std::log10(amplitude / reference);
    }

    /**
     * @brief Convert decibels to amplitude
     *
     * @param db Decibel value
     * @param reference Reference value (default 32768.0 for int16_t)
     * @return Amplitude value
     */
    static double DecibelsToAmplitude(double db, double reference = 32768.0) {
        return reference * std::pow(10.0, db / 20.0);
    }

    /**
     * @brief Generate random byte array
     *
     * @param size Number of bytes
     * @return Vector of random bytes
     */
    static std::vector<uint8_t> GenerateRandomBytes(size_t size) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(std::rand() % 256);
        }
        return data;
    }

    /**
     * @brief Generate test WAV header
     *
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels
     * @param bits_per_sample Bits per sample
     * @param num_samples Number of samples
     * @return WAV header bytes
     */
    static std::vector<uint8_t> GenerateWavHeader(
        uint32_t sample_rate,
        uint16_t channels,
        uint16_t bits_per_sample,
        uint32_t num_samples
    ) {
        std::vector<uint8_t> header(44);

        uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
        uint16_t block_align = channels * (bits_per_sample / 8);
        uint32_t data_size = num_samples * block_align;
        uint32_t file_size = 36 + data_size;

        size_t pos = 0;

        // RIFF header
        header[pos++] = 'R'; header[pos++] = 'I'; header[pos++] = 'F'; header[pos++] = 'F';
        WriteUInt32LE(header, pos, file_size); pos += 4;
        header[pos++] = 'W'; header[pos++] = 'A'; header[pos++] = 'V'; header[pos++] = 'E';

        // fmt chunk
        header[pos++] = 'f'; header[pos++] = 'm'; header[pos++] = 't'; header[pos++] = ' ';
        WriteUInt32LE(header, pos, 16); pos += 4;  // fmt chunk size
        WriteUInt16LE(header, pos, 1); pos += 2;   // audio format (PCM)
        WriteUInt16LE(header, pos, channels); pos += 2;
        WriteUInt32LE(header, pos, sample_rate); pos += 4;
        WriteUInt32LE(header, pos, byte_rate); pos += 4;
        WriteUInt16LE(header, pos, block_align); pos += 2;
        WriteUInt16LE(header, pos, bits_per_sample); pos += 2;

        // data chunk
        header[pos++] = 'd'; header[pos++] = 'a'; header[pos++] = 't'; header[pos++] = 'a';
        WriteUInt32LE(header, pos, data_size);

        return header;
    }

    /**
     * @brief Format bytes as hex string
     *
     * @param data Byte data
     * @param max_bytes Maximum bytes to format (0 = all)
     * @return Hex string
     */
    static std::string BytesToHexString(
        const std::vector<uint8_t>& data,
        size_t max_bytes = 16
    ) {
        std::ostringstream oss;
        size_t count = (max_bytes > 0) ? std::min(max_bytes, data.size()) : data.size();

        for (size_t i = 0; i < count; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(data[i]) << " ";
        }

        if (max_bytes > 0 && data.size() > max_bytes) {
            oss << "... (" << (data.size() - max_bytes) << " more)";
        }

        return oss.str();
    }

    /**
     * @brief Measure execution time of a function
     *
     * @param func Function to measure
     * @return Execution time in milliseconds
     */
    template<typename Func>
    static double MeasureExecutionTime(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> duration = end - start;
        return duration.count();
    }

    /**
     * @brief Check if value is within range
     *
     * @param value Value to check
     * @param min Minimum value (inclusive)
     * @param max Maximum value (inclusive)
     * @return True if value is in range
     */
    template<typename T>
    static bool InRange(T value, T min, T max) {
        return value >= min && value <= max;
    }

    /**
     * @brief Clamp value to range
     *
     * @param value Value to clamp
     * @param min Minimum value
     * @param max Maximum value
     * @return Clamped value
     */
    template<typename T>
    static T Clamp(T value, T min, T max) {
        return std::max(min, std::min(value, max));
    }

private:
    /**
     * @brief Write uint32_t in little-endian format
     */
    static void WriteUInt32LE(std::vector<uint8_t>& buffer, size_t pos, uint32_t value) {
        buffer[pos + 0] = static_cast<uint8_t>(value & 0xFF);
        buffer[pos + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buffer[pos + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        buffer[pos + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    }

    /**
     * @brief Write uint16_t in little-endian format
     */
    static void WriteUInt16LE(std::vector<uint8_t>& buffer, size_t pos, uint16_t value) {
        buffer[pos + 0] = static_cast<uint8_t>(value & 0xFF);
        buffer[pos + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }
};

/**
 * @class ScopedTimer
 * @brief RAII timer for performance testing
 *
 * Automatically measures time from construction to destruction.
 */
class ScopedTimer {
public:
    /**
     * @brief Constructor - starts timer
     *
     * @param name Timer name for logging
     */
    explicit ScopedTimer(const std::string& name)
        : name_(name),
          start_(std::chrono::high_resolution_clock::now()) {}

    /**
     * @brief Destructor - stops timer and logs result
     */
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start_;
        // Note: In production, this would log to test output
        // For now, we just store the duration
        elapsed_ms_ = duration.count();
    }

    /**
     * @brief Get elapsed time
     *
     * @return Elapsed time in milliseconds
     */
    double GetElapsed() const { return elapsed_ms_; }

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
    double elapsed_ms_ = 0.0;
};

/**
 * @brief Google Test custom matcher for audio buffer comparison
 */
class AudioBufferMatcher {
public:
    AudioBufferMatcher(const std::vector<int16_t>& expected, int16_t tolerance)
        : expected_(expected), tolerance_(tolerance) {}

    bool MatchAndExplain(const std::vector<int16_t>& actual, std::ostream* os) const {
        if (expected_.size() != actual.size()) {
            *os << "Size mismatch: expected " << expected_.size()
                << " but got " << actual.size();
            return false;
        }

        for (size_t i = 0; i < expected_.size(); ++i) {
            int16_t diff = std::abs(expected_[i] - actual[i]);
            if (diff > tolerance_) {
                *os << "Mismatch at index " << i
                    << ": expected " << expected_[i]
                    << " but got " << actual[i]
                    << " (diff = " << diff << ", tolerance = " << tolerance_ << ")";
                return false;
            }
        }

        return true;
    }

    void DescribeTo(std::ostream* os) const {
        *os << "matches audio buffer with tolerance " << tolerance_;
    }

    void DescribeNegationTo(std::ostream* os) const {
        *os << "does not match audio buffer with tolerance " << tolerance_;
    }

private:
    std::vector<int16_t> expected_;
    int16_t tolerance_;
};

/**
 * @brief Helper function to create audio buffer matcher
 */
inline AudioBufferMatcher MatchesAudioBuffer(
    const std::vector<int16_t>& expected,
    int16_t tolerance = 1
) {
    return AudioBufferMatcher(expected, tolerance);
}

} // namespace test
} // namespace ffvoice

#endif // FFVOICE_TESTS_UTILS_TEST_HELPERS_H
