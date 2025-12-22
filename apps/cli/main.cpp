/**
 * @file main.cpp
 * @brief CLI entry point for ffvoice-engine
 */

#include <iostream>
#include <string>
#include <cstdlib>
#include <iomanip>

#include "media/wav_writer.h"
#include "media/flac_writer.h"
#include "utils/signal_generator.h"
#include "utils/logger.h"
#include "audio/audio_capture_device.h"

#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>

void print_usage(const char* program_name) {
    std::cout << "ffvoice-engine v0.1.0 - Low-latency audio capture and recording\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help message\n";
    std::cout << "  --list-devices, -l      List available audio devices\n";
    std::cout << "  --test-wav FILE         Generate test WAV file (440Hz sine wave)\n";
    std::cout << "  --record, -r            Record audio from microphone\n";
    std::cout << "    -d, --device ID       Select audio device (default: auto)\n";
    std::cout << "    -o, --output FILE     Output file path (required)\n";
    std::cout << "    -t, --duration SEC    Recording duration in seconds (0 = unlimited)\n";
    std::cout << "    -f, --format FMT      Output format: wav, flac (default: wav)\n";
    std::cout << "    --sample-rate RATE    Sample rate in Hz (default: 48000)\n";
    std::cout << "    --channels NUM        Number of channels: 1=mono, 2=stereo (default: 1)\n";
    std::cout << "    --compression LEVEL   FLAC compression level 0-8 (default: 5)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " --list-devices\n";
    std::cout << "  " << program_name << " --test-wav test.wav\n";
    std::cout << "  " << program_name << " --record -o recording.wav -t 10\n";
    std::cout << "  " << program_name << " --record -o recording.flac -f flac -t 30\n";
    std::cout << "  " << program_name << " --record -d 1 -o output.wav --channels 2\n";
}

int generate_test_wav(const std::string& filename) {
    using namespace ffvoice;

    std::cout << "Generating test WAV file: " << filename << "\n";
    std::cout << "  Frequency: 440 Hz (A4)\n";
    std::cout << "  Duration: 3 seconds\n";
    std::cout << "  Sample rate: 48000 Hz\n";
    std::cout << "  Channels: mono\n";
    std::cout << "  Bit depth: 16-bit\n\n";

    // Generate 3 seconds of 440Hz sine wave
    auto samples = SignalGenerator::GenerateSineWave(440.0, 3.0, 48000, 0.5);

    std::cout << "Generated " << samples.size() << " samples\n";

    // Write to WAV file
    WavWriter writer;
    if (!writer.Open(filename, 48000, 1, 16)) {
        std::cerr << "Error: Failed to open file for writing: " << filename << "\n";
        return 1;
    }

    size_t written = writer.WriteSamples(samples);
    writer.Close();

    std::cout << "Wrote " << written << " samples to " << filename << "\n";
    std::cout << "Success! Try playing with: afplay " << filename << "\n";

    return 0;
}

int list_devices() {
    using namespace ffvoice;

    std::cout << "Available audio input devices:\n\n";

    auto devices = AudioCaptureDevice::GetDevices();
    if (devices.empty()) {
        std::cout << "No input devices found.\n";
        return 1;
    }

    for (const auto& device : devices) {
        std::cout << "Device " << device.id << ": " << device.name;
        if (device.is_default) {
            std::cout << " [DEFAULT]";
        }
        std::cout << "\n";
        std::cout << "  Channels: " << device.max_input_channels << " in, "
                  << device.max_output_channels << " out\n";
        std::cout << "  Sample rates: ";
        for (size_t i = 0; i < device.supported_sample_rates.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << device.supported_sample_rates[i];
        }
        std::cout << " Hz\n\n";
    }

    return 0;
}

// Global flag for Ctrl+C handling
static std::atomic<bool> g_stop_recording{false};

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nStopping recording...\n";
        g_stop_recording = true;
    }
}

int record_audio(int device_id, int duration, const std::string& output_file,
                 int sample_rate, int channels, const std::string& format,
                 int compression_level) {
    using namespace ffvoice;

    std::cout << "Recording audio:\n";
    std::cout << "  Device: " << device_id << "\n";
    std::cout << "  Sample rate: " << sample_rate << " Hz\n";
    std::cout << "  Channels: " << channels << "\n";
    std::cout << "  Duration: " << (duration == 0 ? "unlimited" : std::to_string(duration) + "s") << "\n";
    std::cout << "  Format: " << format << "\n";
    if (format == "flac") {
        std::cout << "  Compression: level " << compression_level << "\n";
    }
    std::cout << "  Output: " << output_file << "\n\n";

    // Open output file based on format
    WavWriter wav_writer;
    FlacWriter flac_writer;
    bool file_opened = false;

    if (format == "wav") {
        if (wav_writer.Open(output_file, sample_rate, channels, 16)) {
            file_opened = true;
        }
    } else if (format == "flac") {
        if (flac_writer.Open(output_file, sample_rate, channels, 16, compression_level)) {
            file_opened = true;
        }
    } else {
        std::cerr << "Unsupported format: " << format << "\n";
        return 1;
    }

    if (!file_opened) {
        std::cerr << "Failed to open output file: " << output_file << "\n";
        return 1;
    }

    // Open audio device
    AudioCaptureDevice capture;
    if (!capture.Open(device_id, sample_rate, channels, 256)) {
        std::cerr << "Failed to open audio device\n";
        return 1;
    }

    // Set up Ctrl+C handler
    std::signal(SIGINT, signal_handler);

    size_t total_samples = 0;

    // Start capturing with format-specific callback
    bool success = capture.Start([&](const int16_t* samples, size_t num_samples) {
        if (format == "wav") {
            wav_writer.WriteSamples(samples, num_samples);
        } else if (format == "flac") {
            flac_writer.WriteSamples(samples, num_samples);
        }
        total_samples += num_samples;
    });

    if (!success) {
        std::cerr << "Failed to start audio capture\n";
        return 1;
    }

    std::cout << "Recording... (Press Ctrl+C to stop)\n";

    // Record for specified duration or until Ctrl+C
    auto start_time = std::chrono::steady_clock::now();

    while (!g_stop_recording) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (duration > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

            if (elapsed_seconds >= duration) {
                break;
            }

            // Print progress
            std::cout << "\rRecording: " << elapsed_seconds << "s / " << duration << "s" << std::flush;
        }
    }

    std::cout << "\n";

    // Stop and cleanup
    capture.Stop();
    capture.Close();

    if (format == "wav") {
        wav_writer.Close();
    } else if (format == "flac") {
        flac_writer.Close();
    }

    double duration_sec = static_cast<double>(total_samples) / (sample_rate * channels);
    std::cout << "\nRecording complete!\n";
    std::cout << "  Captured: " << total_samples << " samples ("
              << duration_sec << " seconds)\n";
    std::cout << "  Saved to: " << output_file << "\n";

    if (format == "flac") {
        double ratio = flac_writer.GetCompressionRatio();
        std::cout << "  Compression ratio: " << std::fixed << std::setprecision(2)
                  << ratio << "x\n";
    }

    std::cout << "\nPlay with: afplay " << output_file << "\n";

    return 0;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments (simplified for now)
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--help" || arg1 == "-h") {
        print_usage(argv[0]);
        return 0;
    }

    if (arg1 == "--test-wav") {
        if (argc < 3) {
            std::cerr << "Error: --test-wav requires a filename\n";
            std::cerr << "Usage: " << argv[0] << " --test-wav output.wav\n";
            return 1;
        }
        return generate_test_wav(argv[2]);
    }

    if (arg1 == "--list-devices" || arg1 == "-l") {
        return list_devices();
    }

    if (arg1 == "--record" || arg1 == "-r") {
        // Parse arguments
        int device_id = -1;  // -1 = default device
        int duration = 0;    // 0 = unlimited
        std::string output_file;
        std::string format = "wav";  // default format
        int sample_rate = 48000;
        int channels = 1;
        int compression_level = 5;  // FLAC compression level (0-8)

        // Simple argument parsing
        for (int i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) break;

            std::string arg = argv[i];
            std::string value = argv[i + 1];

            if (arg == "-d" || arg == "--device") {
                device_id = std::stoi(value);
            } else if (arg == "-t" || arg == "--duration") {
                duration = std::stoi(value);
            } else if (arg == "-o" || arg == "--output") {
                output_file = value;
            } else if (arg == "-f" || arg == "--format") {
                format = value;
            } else if (arg == "--sample-rate") {
                sample_rate = std::stoi(value);
            } else if (arg == "--channels") {
                channels = std::stoi(value);
            } else if (arg == "--compression") {
                compression_level = std::stoi(value);
            }
        }

        if (output_file.empty()) {
            std::cerr << "Error: Output file required\n";
            std::cerr << "Usage: " << argv[0] << " --record -o output.wav [OPTIONS]\n";
            return 1;
        }

        // Auto-detect format from file extension if not specified explicitly
        if (format == "wav" && output_file.size() > 5 &&
            output_file.substr(output_file.size() - 5) == ".flac") {
            format = "flac";
        }

        return record_audio(device_id, duration, output_file, sample_rate,
                           channels, format, compression_level);
    }

    std::cout << "ffvoice-engine - Audio recording starting...\n";
    std::cout << "TODO: Implement audio capture and recording\n";

    return 0;
}
