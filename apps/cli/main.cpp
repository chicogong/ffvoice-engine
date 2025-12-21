/**
 * @file main.cpp
 * @brief CLI entry point for ffvoice-engine
 */

#include <iostream>
#include <string>
#include <cstdlib>

void print_usage(const char* program_name) {
    std::cout << "ffvoice-engine v0.1.0 - Low-latency audio capture and recording\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help message\n";
    std::cout << "  --list-devices, -l      List available audio devices\n";
    std::cout << "  --device ID, -d ID      Select audio device (default: 0)\n";
    std::cout << "  --duration SEC, -t SEC  Recording duration in seconds (0 = unlimited)\n";
    std::cout << "  --format FMT, -f FMT    Output format: wav, flac (default: wav)\n";
    std::cout << "  --output FILE, -o FILE  Output file path (required for recording)\n";
    std::cout << "  --sample-rate RATE      Sample rate in Hz (default: 48000)\n";
    std::cout << "  --channels NUM          Number of channels 1=mono, 2=stereo (default: 1)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " --list-devices\n";
    std::cout << "  " << program_name << " -d 0 -t 10 -o recording.wav\n";
    std::cout << "  " << program_name << " -f flac -t 30 -o recording.flac\n";
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

    if (arg1 == "--list-devices" || arg1 == "-l") {
        std::cout << "Listing audio devices...\n";
        std::cout << "TODO: Implement device enumeration\n";
        return 0;
    }

    std::cout << "ffvoice-engine - Audio recording starting...\n";
    std::cout << "TODO: Implement audio capture and recording\n";

    return 0;
}
