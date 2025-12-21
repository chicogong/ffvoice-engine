/**
 * @file test_audio_file_writer.cpp
 * @brief Unit tests for AudioFileWriter class
 * @details RED Phase - These tests are designed to FAIL initially
 *          as the production code hasn't been implemented yet
 */

#include <gtest/gtest.h>
#include "media/audio_file_writer.h"
#include "mocks/mock_file_system.h"
#include "fixtures/audio_test_fixture.h"
#include "utils/test_signal_generator.h"
#include "utils/test_helpers.h"
#include <fstream>
#include <filesystem>

namespace ffvoice {
namespace test {

namespace fs = std::filesystem;

/**
 * Test Suite: AudioFileWriterTest
 * Tests audio file writing functionality
 */
class AudioFileWriterTest : public AudioTestFixture {
protected:
    std::unique_ptr<AudioFileWriter> writer_;
    std::unique_ptr<MockFileSystem> mock_fs_;
    std::string test_output_dir_;

    void SetUp() override {
        AudioTestFixture::SetUp();
        writer_ = std::make_unique<AudioFileWriter>();
        mock_fs_ = std::make_unique<MockFileSystem>();

        // Create temporary test directory
        test_output_dir_ = "/tmp/ffvoice_test_" + std::to_string(getpid());
        fs::create_directories(test_output_dir_);
    }

    void TearDown() override {
        // Clean up test files
        if (fs::exists(test_output_dir_)) {
            fs::remove_all(test_output_dir_);
        }
        AudioTestFixture::TearDown();
    }

    std::string getTestFilePath(const std::string& filename) {
        return test_output_dir_ + "/" + filename;
    }
};

// ============================================================================
// WAV Format Tests
// ============================================================================

TEST_F(AudioFileWriterTest, WriteValidWAVFile16Bit) {
    // UT-WR-001: Write valid WAV file (16-bit PCM)
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 44100;
    config.channels = 2;
    config.bit_depth = 16;

    std::string output_path = getTestFilePath("test_16bit.wav");

    EXPECT_TRUE(writer_->open(output_path, config))
        << "Should open WAV file for writing";

    // Generate test signal
    TestSignalGenerator generator;
    auto sine_wave = generator.generateSineWave(440.0f, 44100, 1.0f, 2);

    // Write audio data
    EXPECT_TRUE(writer_->write(sine_wave.data(), sine_wave.size()))
        << "Should write audio data";

    EXPECT_TRUE(writer_->close())
        << "Should close file successfully";

    // Verify file exists and has correct size
    EXPECT_TRUE(fs::exists(output_path))
        << "Output file should exist";

    auto file_size = fs::file_size(output_path);
    size_t expected_size = 44 + sine_wave.size() * sizeof(int16_t); // WAV header + data
    EXPECT_NEAR(file_size, expected_size, 100)
        << "File size should match expected";

    // Verify WAV header
    std::ifstream file(output_path, std::ios::binary);
    char riff_header[4];
    file.read(riff_header, 4);
    EXPECT_EQ(std::string(riff_header, 4), "RIFF")
        << "Should have RIFF header";
}

TEST_F(AudioFileWriterTest, WriteValidWAVFile24Bit) {
    // UT-WR-002: Write valid WAV file (24-bit PCM)
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 48000;
    config.channels = 2;
    config.bit_depth = 24;

    std::string output_path = getTestFilePath("test_24bit.wav");

    EXPECT_TRUE(writer_->open(output_path, config))
        << "Should open 24-bit WAV file";

    TestSignalGenerator generator;
    auto sine_wave = generator.generateSineWave(1000.0f, 48000, 0.5f, 2);

    EXPECT_TRUE(writer_->write(sine_wave.data(), sine_wave.size()))
        << "Should write 24-bit audio data";

    EXPECT_TRUE(writer_->close())
        << "Should close file successfully";

    // Verify file properties
    AudioFileReader reader;
    ASSERT_TRUE(reader.open(output_path));

    EXPECT_EQ(reader.getSampleRate(), 48000);
    EXPECT_EQ(reader.getChannels(), 2);
    EXPECT_EQ(reader.getBitDepth(), 24);
}

TEST_F(AudioFileWriterTest, VerifyWAVHeaderCorrectness) {
    // UT-WR-003: Verify WAV header correctness
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 44100;
    config.channels = 1;
    config.bit_depth = 16;

    std::string output_path = getTestFilePath("test_header.wav");

    ASSERT_TRUE(writer_->open(output_path, config));

    // Write known amount of data
    const size_t num_samples = 44100; // 1 second
    std::vector<float> data(num_samples, 0.0f);
    ASSERT_TRUE(writer_->write(data.data(), data.size()));
    ASSERT_TRUE(writer_->close());

    // Parse WAV header manually
    std::ifstream file(output_path, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    struct WAVHeader {
        char riff[4];
        uint32_t file_size;
        char wave[4];
        char fmt[4];
        uint32_t fmt_size;
        uint16_t audio_format;
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
        char data[4];
        uint32_t data_size;
    } header;

    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    EXPECT_EQ(std::string(header.riff, 4), "RIFF");
    EXPECT_EQ(std::string(header.wave, 4), "WAVE");
    EXPECT_EQ(std::string(header.fmt, 4), "fmt ");
    EXPECT_EQ(std::string(header.data, 4), "data");

    EXPECT_EQ(header.audio_format, 1) << "Should be PCM format";
    EXPECT_EQ(header.num_channels, 1);
    EXPECT_EQ(header.sample_rate, 44100);
    EXPECT_EQ(header.bits_per_sample, 16);
    EXPECT_EQ(header.byte_rate, 44100 * 1 * 2) << "ByteRate = SampleRate * Channels * BytesPerSample";
    EXPECT_EQ(header.block_align, 1 * 2) << "BlockAlign = Channels * BytesPerSample";
    EXPECT_EQ(header.data_size, num_samples * 2) << "Data size should match written samples";
}

TEST_F(AudioFileWriterTest, HandleLargeWAVFiles) {
    // UT-WR-004: Handle large files (> 2GB)
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 48000;
    config.channels = 2;
    config.bit_depth = 16;

    std::string output_path = getTestFilePath("test_large.wav");

    ASSERT_TRUE(writer_->open(output_path, config));

    // Write data in chunks to simulate large file
    const size_t chunk_size = 48000 * 2; // 1 second stereo
    std::vector<float> chunk(chunk_size, 0.0f);

    // Write 100 seconds of audio (should be < 2GB for standard WAV)
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(writer_->write(chunk.data(), chunk.size()))
            << "Should write chunk " << i;
    }

    EXPECT_TRUE(writer_->close());

    // Verify file size
    auto file_size = fs::file_size(output_path);
    size_t expected_size = 44 + (100 * chunk_size * sizeof(int16_t));
    EXPECT_NEAR(file_size, expected_size, 1000);

    // Test > 4GB (should switch to WAV64 or error)
    std::string large_output = getTestFilePath("test_4gb.wav");
    config.enable_wav64 = true; // Enable WAV64 for large files

    ASSERT_TRUE(writer_->open(large_output, config));

    // Note: Actually writing 4GB would be slow, so we test the API
    EXPECT_TRUE(writer_->supportsLargeFiles())
        << "Should support large files with WAV64";
}

TEST_F(AudioFileWriterTest, WriteMonoAndStereoWAV) {
    // UT-WR-005: Write mono and stereo WAV
    TestSignalGenerator generator;

    // Test mono
    {
        AudioFileConfig config;
        config.format = AudioFileFormat::WAV;
        config.sample_rate = 44100;
        config.channels = 1; // Mono
        config.bit_depth = 16;

        std::string output_path = getTestFilePath("test_mono.wav");

        ASSERT_TRUE(writer_->open(output_path, config));

        auto mono_signal = generator.generateSineWave(440.0f, 44100, 1.0f, 1);
        EXPECT_TRUE(writer_->write(mono_signal.data(), mono_signal.size()));
        EXPECT_TRUE(writer_->close());

        AudioFileReader reader;
        ASSERT_TRUE(reader.open(output_path));
        EXPECT_EQ(reader.getChannels(), 1);
    }

    // Test stereo
    {
        AudioFileConfig config;
        config.format = AudioFileFormat::WAV;
        config.sample_rate = 44100;
        config.channels = 2; // Stereo
        config.bit_depth = 16;

        std::string output_path = getTestFilePath("test_stereo.wav");

        ASSERT_TRUE(writer_->open(output_path, config));

        auto stereo_signal = generator.generateStereoSignal(440.0f, 880.0f, 44100, 1.0f);
        EXPECT_TRUE(writer_->write(stereo_signal.data(), stereo_signal.size()));
        EXPECT_TRUE(writer_->close());

        AudioFileReader reader;
        ASSERT_TRUE(reader.open(output_path));
        EXPECT_EQ(reader.getChannels(), 2);
    }
}

// ============================================================================
// FLAC Format Tests
// ============================================================================

TEST_F(AudioFileWriterTest, WriteValidFLACFile) {
    // UT-WR-006: Write valid FLAC file (compression level 5)
    AudioFileConfig config;
    config.format = AudioFileFormat::FLAC;
    config.sample_rate = 48000;
    config.channels = 2;
    config.bit_depth = 16;
    config.flac_compression_level = 5;

    std::string output_path = getTestFilePath("test.flac");

    EXPECT_TRUE(writer_->open(output_path, config))
        << "Should open FLAC file for writing";

    TestSignalGenerator generator;
    auto sine_wave = generator.generateSineWave(440.0f, 48000, 2.0f, 2);

    EXPECT_TRUE(writer_->write(sine_wave.data(), sine_wave.size()))
        << "Should write FLAC audio data";

    EXPECT_TRUE(writer_->close())
        << "Should close FLAC file";

    // Verify file exists and is valid FLAC
    EXPECT_TRUE(fs::exists(output_path));

    // Check FLAC signature
    std::ifstream file(output_path, std::ios::binary);
    char flac_header[4];
    file.read(flac_header, 4);
    EXPECT_EQ(std::string(flac_header, 4), "fLaC")
        << "Should have FLAC header";
}

TEST_F(AudioFileWriterTest, TestFLACCompressionLevels) {
    // UT-WR-007: Test compression levels 0-8
    TestSignalGenerator generator;
    auto test_signal = generator.generateWhiteNoise(48000, 1.0f, 2); // 1 second white noise

    std::vector<size_t> file_sizes;

    for (int level = 0; level <= 8; ++level) {
        AudioFileConfig config;
        config.format = AudioFileFormat::FLAC;
        config.sample_rate = 48000;
        config.channels = 2;
        config.bit_depth = 16;
        config.flac_compression_level = level;

        std::string output_path = getTestFilePath("test_level_" + std::to_string(level) + ".flac");

        ASSERT_TRUE(writer_->open(output_path, config))
            << "Should open FLAC with compression level " << level;

        ASSERT_TRUE(writer_->write(test_signal.data(), test_signal.size()));
        ASSERT_TRUE(writer_->close());

        file_sizes.push_back(fs::file_size(output_path));
    }

    // Verify compression effectiveness (higher levels = smaller files)
    for (size_t i = 1; i < file_sizes.size(); ++i) {
        EXPECT_LE(file_sizes[i], file_sizes[i-1] * 1.1) // Allow 10% variance
            << "Higher compression level should not significantly increase file size";
    }

    // Level 0 should be notably larger than level 8
    EXPECT_GT(file_sizes[0], file_sizes[8] * 1.2)
        << "Uncompressed should be notably larger than maximum compression";
}

TEST_F(AudioFileWriterTest, VerifyFLACMetadata) {
    // UT-WR-008: Verify FLAC metadata
    AudioFileConfig config;
    config.format = AudioFileFormat::FLAC;
    config.sample_rate = 44100;
    config.channels = 2;
    config.bit_depth = 24;
    config.flac_compression_level = 5;

    // Add metadata
    config.metadata["TITLE"] = "Test Recording";
    config.metadata["ARTIST"] = "FFVoice Engine";
    config.metadata["DATE"] = "2024";
    config.metadata["COMMENT"] = "Unit test file";

    std::string output_path = getTestFilePath("test_metadata.flac");

    ASSERT_TRUE(writer_->open(output_path, config));

    TestSignalGenerator generator;
    auto signal = generator.generateSineWave(440.0f, 44100, 1.0f, 2);
    ASSERT_TRUE(writer_->write(signal.data(), signal.size()));
    ASSERT_TRUE(writer_->close());

    // Read back metadata
    AudioFileReader reader;
    ASSERT_TRUE(reader.open(output_path));

    auto metadata = reader.getMetadata();
    EXPECT_EQ(metadata["TITLE"], "Test Recording");
    EXPECT_EQ(metadata["ARTIST"], "FFVoice Engine");
    EXPECT_EQ(metadata["DATE"], "2024");
    EXPECT_EQ(metadata["COMMENT"], "Unit test file");
}

TEST_F(AudioFileWriterTest, CompareFLACFileSizes) {
    // UT-WR-009: Compare file sizes across compression levels
    TestSignalGenerator generator;

    // Generate different types of audio to test compression
    struct TestCase {
        std::string name;
        std::vector<float> signal;
    };

    std::vector<TestCase> test_cases = {
        {"silence", std::vector<float>(48000 * 2, 0.0f)},
        {"sine", generator.generateSineWave(440.0f, 48000, 1.0f, 2)},
        {"noise", generator.generateWhiteNoise(48000, 1.0f, 2)},
        {"complex", generator.generateComplexTone({440.0f, 880.0f, 1320.0f}, 48000, 1.0f, 2)}
    };

    for (const auto& test : test_cases) {
        size_t uncompressed_size = 0;
        size_t compressed_size = 0;

        // Level 0 (fastest, least compression)
        {
            AudioFileConfig config;
            config.format = AudioFileFormat::FLAC;
            config.sample_rate = 48000;
            config.channels = 2;
            config.flac_compression_level = 0;

            std::string path = getTestFilePath(test.name + "_level0.flac");
            ASSERT_TRUE(writer_->open(path, config));
            ASSERT_TRUE(writer_->write(test.signal.data(), test.signal.size()));
            ASSERT_TRUE(writer_->close());
            uncompressed_size = fs::file_size(path);
        }

        // Level 8 (slowest, best compression)
        {
            AudioFileConfig config;
            config.format = AudioFileFormat::FLAC;
            config.sample_rate = 48000;
            config.channels = 2;
            config.flac_compression_level = 8;

            std::string path = getTestFilePath(test.name + "_level8.flac");
            ASSERT_TRUE(writer_->open(path, config));
            ASSERT_TRUE(writer_->write(test.signal.data(), test.signal.size()));
            ASSERT_TRUE(writer_->close());
            compressed_size = fs::file_size(path);
        }

        float compression_ratio = static_cast<float>(uncompressed_size) / compressed_size;

        // Silence should compress extremely well
        if (test.name == "silence") {
            EXPECT_GT(compression_ratio, 10.0f)
                << "Silence should have high compression ratio";
        }
        // Noise should compress poorly
        else if (test.name == "noise") {
            EXPECT_LT(compression_ratio, 1.5f)
                << "White noise should have low compression ratio";
        }
    }
}

// ============================================================================
// File Operations Tests
// ============================================================================

TEST_F(AudioFileWriterTest, CreateOutputFileSuccessfully) {
    // UT-WR-010: Create output file successfully
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 44100;
    config.channels = 2;

    std::string output_path = getTestFilePath("test_create.wav");

    EXPECT_FALSE(fs::exists(output_path))
        << "File should not exist initially";

    EXPECT_TRUE(writer_->open(output_path, config))
        << "Should create new file";

    EXPECT_TRUE(writer_->isOpen())
        << "Writer should be open";

    EXPECT_TRUE(fs::exists(output_path))
        << "File should exist after opening";

    EXPECT_TRUE(writer_->close());
}

TEST_F(AudioFileWriterTest, HandleFileCreationFailure) {
    // UT-WR-011: Handle file creation failure
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 44100;
    config.channels = 2;

    // Try to write to non-existent directory
    std::string invalid_path = "/nonexistent/directory/test.wav";

    EXPECT_FALSE(writer_->open(invalid_path, config))
        << "Should fail to create file in non-existent directory";

    auto error = writer_->getLastError();
    EXPECT_FALSE(error.empty())
        << "Should provide error message";
    EXPECT_TRUE(error.find("directory") != std::string::npos ||
                error.find("path") != std::string::npos)
        << "Error should mention directory/path issue";

    // Try to write to read-only location
    std::string readonly_path = "/sys/test.wav"; // System directory (read-only)

    EXPECT_FALSE(writer_->open(readonly_path, config))
        << "Should fail to create file in read-only location";
}

TEST_F(AudioFileWriterTest, OverwriteExistingFile) {
    // UT-WR-012: Overwrite existing file
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 44100;
    config.channels = 1;

    std::string output_path = getTestFilePath("test_overwrite.wav");

    // Create initial file
    {
        ASSERT_TRUE(writer_->open(output_path, config));
        std::vector<float> data(1000, 0.5f);
        ASSERT_TRUE(writer_->write(data.data(), data.size()));
        ASSERT_TRUE(writer_->close());
    }

    auto original_size = fs::file_size(output_path);

    // Overwrite with different data
    {
        config.overwrite_mode = OverwriteMode::OVERWRITE;
        EXPECT_TRUE(writer_->open(output_path, config))
            << "Should overwrite existing file";

        std::vector<float> data(5000, 0.1f); // Different size
        EXPECT_TRUE(writer_->write(data.data(), data.size()));
        EXPECT_TRUE(writer_->close());
    }

    auto new_size = fs::file_size(output_path);
    EXPECT_NE(original_size, new_size)
        << "File size should change after overwrite";

    // Test with protection mode
    {
        config.overwrite_mode = OverwriteMode::PROTECT;
        EXPECT_FALSE(writer_->open(output_path, config))
            << "Should not overwrite in PROTECT mode";
    }

    // Test with backup mode
    {
        config.overwrite_mode = OverwriteMode::BACKUP;
        EXPECT_TRUE(writer_->open(output_path, config))
            << "Should create backup and overwrite";

        EXPECT_TRUE(fs::exists(output_path + ".bak"))
            << "Should create backup file";
    }
}

TEST_F(AudioFileWriterTest, FlushAndCloseProperly) {
    // UT-WR-013: Flush and close properly
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 48000;
    config.channels = 2;

    std::string output_path = getTestFilePath("test_flush.wav");

    ASSERT_TRUE(writer_->open(output_path, config));

    // Write data in chunks
    TestSignalGenerator generator;
    for (int i = 0; i < 10; ++i) {
        auto chunk = generator.generateSineWave(440.0f + i * 10, 4800, 0.1f, 2);
        EXPECT_TRUE(writer_->write(chunk.data(), chunk.size()));

        // Flush periodically
        if (i % 3 == 0) {
            EXPECT_TRUE(writer_->flush())
                << "Should flush buffer to disk";
        }
    }

    // Get size before close
    writer_->flush();
    auto size_before_close = fs::file_size(output_path);

    EXPECT_TRUE(writer_->close())
        << "Should close file properly";

    auto size_after_close = fs::file_size(output_path);

    // Size might change slightly due to header finalization
    EXPECT_NEAR(size_before_close, size_after_close, 100)
        << "File size should be similar after close";

    // Verify file is valid after close
    AudioFileReader reader;
    EXPECT_TRUE(reader.open(output_path))
        << "File should be readable after proper close";
}

TEST_F(AudioFileWriterTest, CleanupOnError) {
    // UT-WR-014: Cleanup on error
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 48000;
    config.channels = 2;

    std::string output_path = getTestFilePath("test_cleanup.wav");

    ASSERT_TRUE(writer_->open(output_path, config));

    // Simulate error during write by using mock filesystem
    writer_->setFileSystem(mock_fs_.get());
    mock_fs_->setWriteError(true);

    TestSignalGenerator generator;
    auto data = generator.generateSineWave(440.0f, 48000, 1.0f, 2);

    EXPECT_FALSE(writer_->write(data.data(), data.size()))
        << "Write should fail with simulated error";

    // Writer should clean up
    writer_->abort(); // Abort and cleanup

    EXPECT_FALSE(writer_->isOpen())
        << "Writer should be closed after abort";

    // Depending on implementation, file might be deleted or marked invalid
    if (fs::exists(output_path)) {
        auto file_size = fs::file_size(output_path);
        EXPECT_EQ(file_size, 0)
            << "Aborted file should be empty or deleted";
    }
}

// ============================================================================
// Data Integrity Tests
// ============================================================================

TEST_F(AudioFileWriterTest, WriteAndVerifySineWave) {
    // UT-WR-015: Write and verify sine wave
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 48000;
    config.channels = 1;
    config.bit_depth = 16;

    std::string output_path = getTestFilePath("test_sine_verify.wav");

    ASSERT_TRUE(writer_->open(output_path, config));

    // Generate known sine wave
    TestSignalGenerator generator;
    float frequency = 1000.0f;
    auto original_signal = generator.generateSineWave(frequency, 48000, 1.0f, 1);

    ASSERT_TRUE(writer_->write(original_signal.data(), original_signal.size()));
    ASSERT_TRUE(writer_->close());

    // Read back and verify
    AudioFileReader reader;
    ASSERT_TRUE(reader.open(output_path));

    std::vector<float> read_signal(original_signal.size());
    ASSERT_EQ(reader.read(read_signal.data(), read_signal.size()),
              original_signal.size());

    // Analyze the read signal
    AudioAnalyzer analyzer;
    auto detected_freq = analyzer.detectFrequency(read_signal.data(), read_signal.size(), 48000);

    EXPECT_NEAR(detected_freq, frequency, 10.0f)
        << "Detected frequency should match original";

    // Calculate SNR
    auto snr = analyzer.calculateSNR(original_signal.data(), read_signal.data(), original_signal.size());
    EXPECT_GT(snr, 50.0f) << "SNR should be high for lossless encoding";
}

TEST_F(AudioFileWriterTest, VerifySampleCountAccuracy) {
    // UT-WR-016: Verify sample count accuracy
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 44100;
    config.channels = 2;

    std::string output_path = getTestFilePath("test_sample_count.wav");

    ASSERT_TRUE(writer_->open(output_path, config));

    // Write exact number of samples
    const size_t num_samples = 123456;
    std::vector<float> data(num_samples, 0.0f);

    ASSERT_TRUE(writer_->write(data.data(), data.size()));
    ASSERT_TRUE(writer_->close());

    // Read back and verify count
    AudioFileReader reader;
    ASSERT_TRUE(reader.open(output_path));

    EXPECT_EQ(reader.getTotalSamples(), num_samples / 2) // Stereo: frames = samples / channels
        << "Sample count should match exactly";

    EXPECT_EQ(reader.getDuration(), static_cast<double>(num_samples / 2) / 44100.0)
        << "Duration should match sample count";
}

TEST_F(AudioFileWriterTest, VerifyChannelInterleaving) {
    // UT-WR-017: Verify channel interleaving
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 48000;
    config.channels = 2;

    std::string output_path = getTestFilePath("test_interleaving.wav");

    ASSERT_TRUE(writer_->open(output_path, config));

    // Create distinct signals for left and right
    TestSignalGenerator generator;
    auto left_channel = generator.generateSineWave(440.0f, 48000, 1.0f, 1);
    auto right_channel = generator.generateSineWave(880.0f, 48000, 1.0f, 1);

    // Interleave manually
    std::vector<float> interleaved;
    for (size_t i = 0; i < left_channel.size(); ++i) {
        interleaved.push_back(left_channel[i]);
        interleaved.push_back(right_channel[i]);
    }

    ASSERT_TRUE(writer_->write(interleaved.data(), interleaved.size()));
    ASSERT_TRUE(writer_->close());

    // Read back and verify
    AudioFileReader reader;
    ASSERT_TRUE(reader.open(output_path));

    std::vector<float> read_data(interleaved.size());
    reader.read(read_data.data(), read_data.size());

    // Verify interleaving pattern
    for (size_t i = 0; i < left_channel.size(); ++i) {
        EXPECT_NEAR(read_data[i * 2], left_channel[i], 0.01f)
            << "Left channel sample " << i << " should match";
        EXPECT_NEAR(read_data[i * 2 + 1], right_channel[i], 0.01f)
            << "Right channel sample " << i << " should match";
    }
}

TEST_F(AudioFileWriterTest, TestWithZeroFilledBuffers) {
    // UT-WR-018: Test with zero-filled buffers
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 48000;
    config.channels = 1;

    std::string output_path = getTestFilePath("test_zeros.wav");

    ASSERT_TRUE(writer_->open(output_path, config));

    // Write silence
    std::vector<float> silence(48000, 0.0f); // 1 second of silence
    ASSERT_TRUE(writer_->write(silence.data(), silence.size()));
    ASSERT_TRUE(writer_->close());

    // Verify the file
    AudioFileReader reader;
    ASSERT_TRUE(reader.open(output_path));

    std::vector<float> read_data(silence.size());
    reader.read(read_data.data(), read_data.size());

    // All samples should be zero
    for (size_t i = 0; i < read_data.size(); ++i) {
        EXPECT_EQ(read_data[i], 0.0f)
            << "Sample " << i << " should be zero";
    }
}

TEST_F(AudioFileWriterTest, TestWithFullScaleAudio) {
    // UT-WR-019: Test with full-scale audio
    AudioFileConfig config;
    config.format = AudioFileFormat::WAV;
    config.sample_rate = 48000;
    config.channels = 2;
    config.bit_depth = 16;

    std::string output_path = getTestFilePath("test_fullscale.wav");

    ASSERT_TRUE(writer_->open(output_path, config));

    // Create full-scale square wave
    std::vector<float> fullscale;
    for (int i = 0; i < 48000; ++i) {
        float value = (i % 100 < 50) ? 1.0f : -1.0f;
        fullscale.push_back(value); // Left
        fullscale.push_back(value); // Right
    }

    ASSERT_TRUE(writer_->write(fullscale.data(), fullscale.size()));
    ASSERT_TRUE(writer_->close());

    // Read back and verify no clipping occurred
    AudioFileReader reader;
    ASSERT_TRUE(reader.open(output_path));

    std::vector<float> read_data(fullscale.size());
    reader.read(read_data.data(), read_data.size());

    int clipped_samples = 0;
    for (size_t i = 0; i < read_data.size(); ++i) {
        if (std::abs(read_data[i]) > 1.0f) {
            clipped_samples++;
        }
        // Should be close to ±1.0
        EXPECT_NEAR(std::abs(read_data[i]), 1.0f, 0.01f)
            << "Full scale samples should be preserved";
    }

    EXPECT_EQ(clipped_samples, 0) << "No samples should be clipped";
}

} // namespace test
} // namespace ffvoice