/**
 * @file main.cpp
 * @brief CLI entry point for ffvoice-engine
 */

#include <iostream>
#include <string>
#include <cstdlib>

#include "media/wav_writer.h"
#include "utils/signal_generator.h"
#include "utils/logger.h"

void print_usage(const char* program_name) {
    std::cout << "ffvoice-engine v0.1.0 - Low-latency audio capture and recording\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help message\n";
    std::cout << "  --list-devices, -l      List available audio devices\n";
    std::cout << "  --test-wav FILE         Generate test WAV file (440Hz sine wave)\n";
    std::cout << "  --device ID, -d ID      Select audio device (default: 0)\n";
    std::cout << "  --duration SEC, -t SEC  Recording duration in seconds (0 = unlimited)\n";
    std::cout << "  --format FMT, -f FMT    Output format: wav, flac (default: wav)\n";
    std::cout << "  --output FILE, -o FILE  Output file path (required for recording)\n";
    std::cout << "  --sample-rate RATE      Sample rate in Hz (default: 48000)\n";
    std::cout << "  --channels NUM          Number of channels 1=mono, 2=stereo (default: 1)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " --test-wav test.wav\n";
    std::cout << "  " << program_name << " --list-devices\n";
    std::cout << "  " << program_name << " -d 0 -t 10 -o recording.wav\n";
    std::cout << "  " << program_name << " -f flac -t 30 -o recording.flac\n";
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
        std::cout << "Listing audio devices...\n";
        std::cout << "TODO: Implement device enumeration\n";
        return 0;
    }

    std::cout << "ffvoice-engine - Audio recording starting...\n";
    std::cout << "TODO: Implement audio capture and recording\n";

    return 0;
}
