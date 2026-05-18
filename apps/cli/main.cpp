/**
 * @file main.cpp
 * @brief CLI entry point for ffvoice-engine
 */

#include "audio/audio_capture_device.h"
#include "audio/audio_processor.h"
#include "media/flac_writer.h"
#include "media/wav_writer.h"
#include "utils/logger.h"
#include "utils/signal_generator.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

#ifdef ENABLE_RNNOISE
    #include "audio/rnnoise_processor.h"
#endif

#ifdef ENABLE_WHISPER
    #include "audio/vad_segmenter.h"
    #include "audio/whisper_processor.h"
    #include "utils/subtitle_generator.h"
#endif

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

// Version is injected by CMake (single source of truth); fall back for non-CMake builds.
#ifndef FFVOICE_VERSION
    #define FFVOICE_VERSION "0.7.0"
#endif

// ---------------------------------------------------------------------------
// Exit-code taxonomy
// ---------------------------------------------------------------------------
constexpr int EXIT_OK = 0;
constexpr int EXIT_BAD_ARGS = 2;
constexpr int EXIT_NOT_FOUND = 3;
constexpr int EXIT_RUNTIME = 4;

// ---------------------------------------------------------------------------
// Global JSON mode flag (set in a pre-pass before dispatch)
// ---------------------------------------------------------------------------
static bool g_json_mode = false;

// Mutex that guards every stdout write when --json is active.  The audio
// callback thread emits "segment" events while the main thread emits
// "progress" events; both must write atomically.
static std::mutex g_stdout_mutex;

// ---------------------------------------------------------------------------
// JSON helper: escape a raw string for embedding as a JSON string value.
// (Mirrors EscapeJSON in subtitle_generator.cpp — kept local to avoid
//  coupling to a private helper.)
// ---------------------------------------------------------------------------
static std::string json_escape(const std::string& input) {
    std::ostringstream oss;
    for (unsigned char c : input) {
        switch (c) {
            case '"':
                oss << "\\\"";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                if (c < 0x20) {
                    oss << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                        << static_cast<int>(c) << std::dec;
                } else {
                    oss << c;
                }
                break;
        }
    }
    return oss.str();
}

// ---------------------------------------------------------------------------
// emit_error(): write an error report.
//   JSON mode  → {"error":{"code":N,"message":"..."}}  on stderr
//   Plain mode → plain prose on stderr
// ---------------------------------------------------------------------------
static void emit_error(int code, const std::string& message) {
    if (g_json_mode) {
        std::cerr << "{\"error\":{\"code\":" << code << ",\"message\":\"" << json_escape(message)
                  << "\"}}" << std::endl;
    } else {
        std::cerr << "Error: " << message << "\n";
    }
}

// ---------------------------------------------------------------------------
// emit_json_line(): thread-safe locked write of one JSON line to stdout.
// ---------------------------------------------------------------------------
static void emit_json_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_stdout_mutex);
    std::cout << line << "\n" << std::flush;
}

// Parse an integer CLI argument. On failure prints an error and returns false,
// so the whole value (including trailing junk like "12abc") is rejected cleanly
// instead of crashing with an uncaught std::invalid_argument/std::out_of_range.
bool parse_int_arg(const std::string& value, const std::string& flag, int& out) {
    try {
        size_t pos = 0;
        int parsed = std::stoi(value, &pos);
        if (pos != value.size()) {
            throw std::invalid_argument(value);
        }
        out = parsed;
        return true;
    } catch (const std::exception&) {
        emit_error(EXIT_BAD_ARGS, "invalid integer for " + flag + ": '" + value + "'");
        return false;
    }
}

// Parse a floating-point CLI argument. Same contract as parse_int_arg().
bool parse_float_arg(const std::string& value, const std::string& flag, float& out) {
    try {
        size_t pos = 0;
        float parsed = std::stof(value, &pos);
        if (pos != value.size()) {
            throw std::invalid_argument(value);
        }
        out = parsed;
        return true;
    } catch (const std::exception&) {
        emit_error(EXIT_BAD_ARGS, "invalid number for " + flag + ": '" + value + "'");
        return false;
    }
}

void print_usage(const char* program_name) {
    std::cout << "ffvoice-engine v" FFVOICE_VERSION
                 " - Low-latency audio capture and recording\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help message\n";
    std::cout << "  --version, -V           Print version and exit\n";
    std::cout << "  --json                  Machine-readable JSON output mode\n";
    std::cout << "  --list-devices, -l      List available audio devices\n";
    std::cout << "  --test-wav FILE         Generate test WAV file (440Hz sine wave)\n";
    std::cout << "  --record, -r            Record audio from microphone\n";
    std::cout << "    -d, --device ID       Select audio device (default: auto)\n";
    std::cout << "    -o, --output FILE     Output file path (required; use - for stdout)\n";
    std::cout << "    -t, --duration SEC    Recording duration in seconds (0 = unlimited)\n";
    std::cout << "    -f, --format FMT      Output format: wav, flac (default: wav)\n";
    std::cout << "    --sample-rate RATE    Sample rate in Hz (default: 48000)\n";
    std::cout << "    --channels NUM        Number of channels: 1=mono, 2=stereo (default: 1)\n";
    std::cout << "    --compression LEVEL   FLAC compression level 0-8 (default: 5)\n";
    std::cout
        << "    --enable-processing   Enable audio processing (normalize + high-pass filter)\n";
    std::cout << "    --normalize           Enable volume normalization\n";
    std::cout << "    --highpass FREQ       Enable high-pass filter at FREQ Hz (default: 80)\n";
#ifdef ENABLE_RNNOISE
    std::cout << "    --rnnoise             Enable RNNoise deep learning noise suppression\n";
    std::cout << "    --rnnoise-vad         Enable RNNoise with VAD (experimental)\n";
#else
    std::cout << "    (RNNoise not available - rebuild with -DENABLE_RNNOISE=ON)\n";
#endif
#ifdef ENABLE_WHISPER
    std::cout << "\n  Whisper ASR (Speech Recognition):\n";
    std::cout << "    --transcribe FILE     Transcribe audio file (offline mode)\n";
    std::cout << "    --format FMT          Subtitle format: txt, srt, vtt, json (default: txt)\n";
    std::cout << "    --language LANG       Language: auto, zh, en, etc. (default: auto)\n";
    std::cout << "    --transcribe-live     Enable real-time transcription during recording\n";
    std::cout << "                          (requires --rnnoise-vad for VAD-based segmentation)\n";
#else
    std::cout << "\n  (Whisper ASR not available - rebuild with -DENABLE_WHISPER=ON)\n";
#endif
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " --version\n";
    std::cout << "  " << program_name << " --list-devices\n";
    std::cout << "  " << program_name << " --list-devices --json\n";
    std::cout << "  " << program_name << " --test-wav test.wav\n";
    std::cout << "  " << program_name << " --record -o recording.wav -t 10\n";
    std::cout << "  " << program_name << " --record -o recording.flac -f flac -t 30\n";
    std::cout << "  " << program_name << " --record -o output.wav --enable-processing -t 20\n";
    std::cout << "  " << program_name << " --record -o clean.flac --normalize --highpass 100\n";
#ifdef ENABLE_RNNOISE
    std::cout << "  " << program_name << " --record -o clean.wav --rnnoise -t 10\n";
    std::cout << "  " << program_name
              << " --record -o studio.flac --rnnoise --highpass 80 --normalize\n";
#endif
#ifdef ENABLE_WHISPER
    std::cout << "  " << program_name << " --transcribe speech.wav -o transcript.txt\n";
    std::cout << "  " << program_name << " --transcribe speech.wav --format srt -o subtitles.srt\n";
    std::cout << "  " << program_name << " --transcribe speech.flac --format vtt --language zh\n";
    std::cout << "  " << program_name
              << " --transcribe speech.wav --format json -o transcript.json\n";
    std::cout << "  " << program_name
              << " --record -o speech.wav --rnnoise-vad --transcribe-live -t 60\n";
    std::cout << "  " << program_name << " --transcribe speech.wav -o -\n";
#endif
}

int generate_test_wav(const std::string& filename) {
    using namespace ffvoice;

    std::cerr << "Generating test WAV file: " << filename << "\n";
    std::cerr << "  Frequency: 440 Hz (A4)\n";
    std::cerr << "  Duration: 3 seconds\n";
    std::cerr << "  Sample rate: 48000 Hz\n";
    std::cerr << "  Channels: mono\n";
    std::cerr << "  Bit depth: 16-bit\n\n";

    // Generate 3 seconds of 440Hz sine wave
    auto samples = SignalGenerator::GenerateSineWave(440.0, 3.0, 48000, 0.5);

    std::cerr << "Generated " << samples.size() << " samples\n";

    // Write to WAV file
    WavWriter writer;
    if (!writer.Open(filename, 48000, 1, 16)) {
        emit_error(EXIT_RUNTIME, "Failed to open file for writing: " + filename);
        return EXIT_RUNTIME;
    }

    size_t written = writer.WriteSamples(samples);
    writer.Close();

    std::cerr << "Wrote " << written << " samples to " << filename << "\n";
    std::cerr << "Success! Try playing with: afplay " << filename << "\n";

    return EXIT_OK;
}

int list_devices() {
    using namespace ffvoice;

    auto devices = AudioCaptureDevice::GetDevices();
    if (devices.empty()) {
        if (g_json_mode) {
            emit_json_line("{\"devices\":[]}");
        } else {
            std::cerr << "No input devices found.\n";
        }
        return EXIT_NOT_FOUND;
    }

    if (g_json_mode) {
        std::ostringstream oss;
        oss << "{\"devices\":[";
        for (size_t d = 0; d < devices.size(); ++d) {
            const auto& dev = devices[d];
            if (d > 0)
                oss << ",";
            oss << "{";
            oss << "\"id\":" << dev.id << ",";
            oss << "\"name\":\"" << json_escape(dev.name) << "\",";
            oss << "\"is_default\":" << (dev.is_default ? "true" : "false") << ",";
            oss << "\"max_input_channels\":" << dev.max_input_channels << ",";
            oss << "\"max_output_channels\":" << dev.max_output_channels << ",";
            oss << "\"supported_sample_rates\":[";
            for (size_t i = 0; i < dev.supported_sample_rates.size(); ++i) {
                if (i > 0)
                    oss << ",";
                oss << dev.supported_sample_rates[i];
            }
            oss << "]}";
        }
        oss << "]}";
        emit_json_line(oss.str());
    } else {
        std::cerr << "Available audio input devices:\n\n";
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
                if (i > 0)
                    std::cout << ", ";
                std::cout << device.supported_sample_rates[i];
            }
            std::cout << " Hz\n\n";
        }
    }

    return EXIT_OK;
}

#ifdef ENABLE_WHISPER
int transcribe_file(const std::string& audio_file, const std::string& output_file,
                    const std::string& format_str, const std::string& language) {
    using namespace ffvoice;

    std::cerr << "Transcribing audio file:\n";
    std::cerr << "  Input: " << audio_file << "\n";
    std::cerr << "  Output: " << output_file << "\n";
    std::cerr << "  Format: " << format_str << "\n";
    std::cerr << "  Language: " << language << "\n\n";

    // Determine output format (resolved before constructing the processor so the
    // JSON format can request per-word timestamps from Whisper).
    SubtitleGenerator::Format format;
    if (format_str == "srt") {
        format = SubtitleGenerator::Format::SRT;
    } else if (format_str == "vtt") {
        format = SubtitleGenerator::Format::VTT;
    } else if (format_str == "json") {
        format = SubtitleGenerator::Format::JSON;
    } else {
        format = SubtitleGenerator::Format::PlainText;
    }

    // Initialize Whisper processor
    WhisperConfig config;
    config.language = language;
    // In JSON mode whisper.cpp's own C-level progress output would pollute stdout.
    config.print_progress = !g_json_mode;
    // JSON output embeds per-word timestamps; other formats leave this at false.
    if (format == SubtitleGenerator::Format::JSON) {
        config.word_timestamps = true;
    }

    WhisperProcessor whisper(config);

    if (!whisper.Initialize()) {
        emit_error(EXIT_RUNTIME, "Failed to initialize Whisper: " + whisper.GetLastError());
        return EXIT_RUNTIME;
    }

    // Transcribe audio file
    std::vector<TranscriptionSegment> segments;
    std::cerr << "Processing... (this may take a while)\n";

    if (!whisper.TranscribeFile(audio_file, segments)) {
        emit_error(EXIT_RUNTIME, "Transcription failed: " + whisper.GetLastError());
        return EXIT_RUNTIME;
    }

    std::cerr << "Transcription complete: " << segments.size() << " segments\n\n";

    // Generate subtitle/transcript output
    if (output_file == "-") {
        // Stream to stdout
        std::string content = SubtitleGenerator::GenerateString(segments, format);
        {
            std::lock_guard<std::mutex> lock(g_stdout_mutex);
            std::cout << content << std::flush;
        }
    } else {
        if (!SubtitleGenerator::Generate(segments, output_file, format)) {
            emit_error(EXIT_RUNTIME, "Failed to generate subtitle file");
            return EXIT_RUNTIME;
        }
        std::cerr << "Success! Transcription saved to: " << output_file << "\n";
    }

    // Print first few segments as preview (to stderr so stdout stays clean)
    std::cerr << "\nPreview (first 3 segments):\n";
    for (size_t i = 0; i < std::min(size_t(3), segments.size()); ++i) {
        std::cerr << "  [" << i << "] " << segments[i].text << "\n";
    }

    return EXIT_OK;
}
#endif

// Global flag for Ctrl+C handling
static std::atomic<bool> g_stop_recording{false};

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cerr << "\nStopping recording...\n";
        g_stop_recording = true;
    }
}

int record_audio(int device_id, int duration, const std::string& output_file, int sample_rate,
                 int channels, const std::string& format, int compression_level,
                 bool enable_normalize, bool enable_highpass, float highpass_freq
#ifdef ENABLE_RNNOISE
                 ,
                 bool enable_rnnoise = false, bool rnnoise_vad = false
#endif
#ifdef ENABLE_WHISPER
                 ,
                 bool transcribe_live = false
#endif
) {
    using namespace ffvoice;

    std::cerr << "Recording audio:\n";
    std::cerr << "  Device: " << device_id << "\n";
    std::cerr << "  Sample rate: " << sample_rate << " Hz\n";
    std::cerr << "  Channels: " << channels << "\n";
    std::cerr << "  Duration: " << (duration == 0 ? "unlimited" : std::to_string(duration) + "s")
              << "\n";
    std::cerr << "  Format: " << format << "\n";
    if (format == "flac") {
        std::cerr << "  Compression: level " << compression_level << "\n";
    }

    // Audio processing
    bool has_processing = enable_normalize || enable_highpass;
#ifdef ENABLE_RNNOISE
    has_processing = has_processing || enable_rnnoise;
#endif
    if (has_processing) {
        std::cerr << "  Audio processing: enabled\n";
        if (enable_highpass) {
            std::cerr << "    - High-pass filter (" << highpass_freq << " Hz)\n";
        }
#ifdef ENABLE_RNNOISE
        if (enable_rnnoise) {
            std::cerr << "    - RNNoise deep learning noise suppression";
            if (rnnoise_vad) {
                std::cerr << " (with VAD)";
            }
            std::cerr << "\n";
        }
#endif
        if (enable_normalize) {
            std::cerr << "    - Volume normalization\n";
        }
    }

    std::cerr << "  Output: " << output_file << "\n";

#ifdef ENABLE_WHISPER
    if (transcribe_live) {
        std::cerr << "  Real-time transcription: enabled\n";
    }
#endif
    std::cerr << "\n";

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
        emit_error(EXIT_BAD_ARGS, "Unsupported format: " + format);
        return EXIT_BAD_ARGS;
    }

    if (!file_opened) {
        emit_error(EXIT_RUNTIME, "Failed to open output file: " + output_file);
        return EXIT_RUNTIME;
    }

    // Setup audio processing chain
    std::unique_ptr<AudioProcessorChain> processor_chain;
#ifdef ENABLE_RNNOISE
    RNNoiseProcessor* rnnoise_ptr = nullptr;  // For VAD access
#endif

    if (has_processing) {
        processor_chain = std::make_unique<AudioProcessorChain>();

        // Processing order: High-pass -> RNNoise -> Normalize
        if (enable_highpass) {
            processor_chain->AddProcessor(std::make_unique<HighPassFilter>(highpass_freq));
        }

#ifdef ENABLE_RNNOISE
        if (enable_rnnoise) {
            RNNoiseConfig config;
            config.enable_vad = rnnoise_vad;
            auto rnnoise = std::make_unique<RNNoiseProcessor>(config);
            rnnoise_ptr = rnnoise.get();  // Store pointer for VAD access
            processor_chain->AddProcessor(std::move(rnnoise));
        }
#endif

        if (enable_normalize) {
            processor_chain->AddProcessor(std::make_unique<VolumeNormalizer>());
        }

        // Initialize the processor chain
        if (!processor_chain->Initialize(sample_rate, channels)) {
            emit_error(EXIT_RUNTIME, "Failed to initialize audio processing");
            return EXIT_RUNTIME;
        }
    }

#ifdef ENABLE_WHISPER
    // Setup real-time transcription (requires RNNoise VAD)
    std::unique_ptr<WhisperProcessor> whisper_processor;
    std::unique_ptr<VADSegmenter> vad_segmenter;
    std::atomic<int> segment_counter{0};

    if (transcribe_live) {
    #ifdef ENABLE_RNNOISE
        if (!rnnoise_vad || !rnnoise_ptr) {
            emit_error(EXIT_BAD_ARGS, "--transcribe-live requires --rnnoise-vad");
            return EXIT_BAD_ARGS;
        }

        // Initialize Whisper processor for real-time transcription
        WhisperConfig whisper_config;
        whisper_config.language = "auto";
        whisper_config.print_progress = false;             // Don't print progress for real-time
        whisper_config.enable_performance_metrics = true;  // Enable performance timing
        whisper_processor = std::make_unique<WhisperProcessor>(whisper_config);

        if (!whisper_processor->Initialize()) {
            emit_error(EXIT_RUNTIME,
                       "Failed to initialize Whisper: " + whisper_processor->GetLastError());
            return EXIT_RUNTIME;
        }

        // Initialize VAD segmenter
        VADSegmenter::Config vad_config;
        vad_config.speech_threshold = 0.5f;
        vad_config.min_speech_frames = 30;        // ~0.3s
        vad_config.min_silence_frames = 50;       // ~0.5s
        vad_config.max_segment_samples = 480000;  // 10s @48kHz
        vad_segmenter = std::make_unique<VADSegmenter>(vad_config);

        std::cerr << "Real-time transcription initialized\n";
        std::cerr << "  Whisper model: loaded\n";
        std::cerr << "  VAD segmentation: enabled\n\n";
    #else
        emit_error(EXIT_BAD_ARGS, "--transcribe-live requires RNNoise support");
        std::cerr << "Rebuild with: cmake -DENABLE_RNNOISE=ON -DENABLE_WHISPER=ON\n";
        return EXIT_BAD_ARGS;
    #endif
    }
#endif

    // Open audio device
    AudioCaptureDevice capture;
    if (!capture.Open(device_id, sample_rate, channels, 256)) {
        emit_error(EXIT_RUNTIME, "Failed to open audio device");
        return EXIT_RUNTIME;
    }

    // Set up Ctrl+C handler
    std::signal(SIGINT, signal_handler);

    size_t total_samples = 0;

    // Buffer for audio processing (if enabled)
    std::vector<int16_t> process_buffer;
    if (has_processing) {
        process_buffer.resize(256 * channels * 4);  // Large enough buffer
    }

    // Emit JSON start event (before the capture loop)
    if (g_json_mode) {
        std::ostringstream oss;
        oss << "{\"event\":\"start\",\"output_file\":\"" << json_escape(output_file)
            << "\",\"sample_rate\":" << sample_rate << ",\"channels\":" << channels
            << ",\"format\":\"" << format << "\"}";
        emit_json_line(oss.str());
    }

    // Start capturing with format-specific callback
    bool success = capture.Start([&](const int16_t* samples, size_t num_samples) {
        // Apply audio processing if enabled
        const int16_t* processed_samples = samples;
        if (has_processing && processor_chain) {
            // Copy to buffer for in-place processing
            if (process_buffer.size() < num_samples) {
                process_buffer.resize(num_samples);
            }
            std::copy(samples, samples + num_samples, process_buffer.data());

            // Process in-place
            processor_chain->Process(process_buffer.data(), num_samples);
            processed_samples = process_buffer.data();
        }

#ifdef ENABLE_WHISPER
        // Real-time transcription: VAD segmentation
        if (transcribe_live && vad_segmenter && whisper_processor) {
    #ifdef ENABLE_RNNOISE
            if (rnnoise_ptr) {
                // Get VAD probability from RNNoise
                float vad_prob = rnnoise_ptr->GetVADProbability();

                // Process frame with VAD segmenter
                vad_segmenter->ProcessFrame(
                    processed_samples, num_samples, vad_prob,
                    [&](const int16_t* segment_samples, size_t segment_size) {
                        // Segment callback: transcribe this segment
                        std::vector<TranscriptionSegment> segments;

                        if (whisper_processor->TranscribeBuffer(segment_samples, segment_size,
                                                                segments)) {
                            // Print transcription results
                            for (const auto& seg : segments) {
                                int idx = segment_counter++;
                                if (g_json_mode) {
                                    std::ostringstream oss;
                                    oss << "{\"event\":\"segment\",\"index\":" << idx
                                        << ",\"text\":\"" << json_escape(seg.text) << "\"}";
                                    emit_json_line(oss.str());
                                } else {
                                    std::lock_guard<std::mutex> lock(g_stdout_mutex);
                                    std::cout << "\n[" << idx << "] " << seg.text << std::flush;
                                }
                            }
                        }
                    });
            }
    #endif
        }
#endif

        // Write processed (or original) samples to file
        if (format == "wav") {
            wav_writer.WriteSamples(processed_samples, num_samples);
        } else if (format == "flac") {
            flac_writer.WriteSamples(processed_samples, num_samples);
        }
        total_samples += num_samples;
    });

    if (!success) {
        emit_error(EXIT_RUNTIME, "Failed to start audio capture");
        return EXIT_RUNTIME;
    }

    if (!g_json_mode) {
        std::cerr << "Recording... (Press Ctrl+C to stop)\n";
    }

    // Record for specified duration or until Ctrl+C
    auto start_time = std::chrono::steady_clock::now();

    while (!g_stop_recording) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (duration > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

            if (elapsed_seconds >= duration) {
                break;
            }

            // Print progress
            if (g_json_mode) {
                std::ostringstream oss;
                oss << "{\"event\":\"progress\",\"elapsed_seconds\":" << elapsed_seconds
                    << ",\"total_seconds\":" << duration << "}";
                emit_json_line(oss.str());
            } else {
                std::cerr << "\rRecording: " << elapsed_seconds << "s / " << duration << "s"
                          << std::flush;
            }
        }
    }

    if (!g_json_mode) {
        std::cerr << "\n";
    }

    // Stop and cleanup
    capture.Stop();
    capture.Close();

    if (format == "wav") {
        wav_writer.Close();
    } else if (format == "flac") {
        flac_writer.Close();
    }

    double duration_sec = static_cast<double>(total_samples) / (sample_rate * channels);

    if (g_json_mode) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "{\"event\":\"complete\",\"total_samples\":" << total_samples
            << ",\"duration_seconds\":" << duration_sec << ",\"output_file\":\""
            << json_escape(output_file) << "\"";
        if (format == "flac") {
            oss << ",\"compression_ratio\":" << std::setprecision(2)
                << flac_writer.GetCompressionRatio();
        }
        oss << "}";
        emit_json_line(oss.str());
    } else {
        std::cerr << "\nRecording complete!\n";
        std::cerr << "  Captured: " << total_samples << " samples (" << duration_sec
                  << " seconds)\n";
        std::cerr << "  Saved to: " << output_file << "\n";

        if (format == "flac") {
            double ratio = flac_writer.GetCompressionRatio();
            std::cerr << "  Compression ratio: " << std::fixed << std::setprecision(2) << ratio
                      << "x\n";
        }

        std::cerr << "\nPlay with: afplay " << output_file << "\n";
    }

    return EXIT_OK;
}

int main(int argc, char* argv[]) {
    // ---------------------------------------------------------------------------
    // Pre-pass: strip --json from argv before command dispatch so that every
    // code path below sees a clean argv without the flag yet can rely on the
    // global g_json_mode being set.
    // ---------------------------------------------------------------------------
    std::vector<char*> filtered_argv;
    filtered_argv.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--json") {
            g_json_mode = true;
        } else {
            filtered_argv.push_back(argv[i]);
        }
    }
    int fargc = static_cast<int>(filtered_argv.size());
    char** fargv = filtered_argv.data();

    // Parse command line arguments
    if (fargc < 2) {
        print_usage(fargv[0]);
        return EXIT_OK;
    }

    std::string arg1 = fargv[1];

    if (arg1 == "--help" || arg1 == "-h") {
        print_usage(fargv[0]);
        return EXIT_OK;
    }

    if (arg1 == "--version" || arg1 == "-V") {
        std::cout << FFVOICE_VERSION << "\n";
        return EXIT_OK;
    }

    if (arg1 == "--test-wav") {
        if (fargc < 3) {
            emit_error(EXIT_BAD_ARGS, "--test-wav requires a filename");
            std::cerr << "Usage: " << fargv[0] << " --test-wav output.wav\n";
            return EXIT_BAD_ARGS;
        }
        return generate_test_wav(fargv[2]);
    }

    if (arg1 == "--list-devices" || arg1 == "-l") {
        return list_devices();
    }

#ifdef ENABLE_WHISPER
    if (arg1 == "--transcribe") {
        // Parse transcription arguments
        if (fargc < 3) {
            emit_error(EXIT_BAD_ARGS, "--transcribe requires an audio file");
            std::cerr << "Usage: " << fargv[0]
                      << " --transcribe input.wav -o output.txt [OPTIONS]\n";
            return EXIT_BAD_ARGS;
        }

        std::string audio_file = fargv[2];
        std::string output_file;
        std::string format = "txt";     // default: plain text
        std::string language = "auto";  // default: auto-detect

        // Parse options
        for (int i = 3; i < fargc; ++i) {
            std::string arg = fargv[i];

            if (i + 1 >= fargc)
                break;
            std::string value = fargv[i + 1];

            if (arg == "-o" || arg == "--output") {
                output_file = value;
                ++i;
            } else if (arg == "--format" || arg == "-f") {
                format = value;
                ++i;
            } else if (arg == "--language") {
                language = value;
                ++i;
            }
        }

        if (output_file.empty()) {
            emit_error(EXIT_BAD_ARGS, "Output file required (-o FILE)");
            return EXIT_BAD_ARGS;
        }

        return transcribe_file(audio_file, output_file, format, language);
    }
#endif

    if (arg1 == "--record" || arg1 == "-r") {
        // Parse arguments
        int device_id = -1;  // -1 = default device
        int duration = 0;    // 0 = unlimited
        std::string output_file;
        std::string format = "wav";  // default format
        int sample_rate = 48000;
        int channels = 1;
        int compression_level = 5;  // FLAC compression level (0-8)

        // Audio processing options
        bool enable_normalize = false;
        bool enable_highpass = false;
        float highpass_freq = 80.0f;  // Default 80 Hz
#ifdef ENABLE_RNNOISE
        bool enable_rnnoise = false;
        bool rnnoise_vad = false;
#endif
#ifdef ENABLE_WHISPER
        bool transcribe_live = false;
#endif

        // Simple argument parsing
        for (int i = 2; i < fargc; ++i) {
            std::string arg = fargv[i];

            // Handle flags without values
            if (arg == "--enable-processing") {
                enable_normalize = true;
                enable_highpass = true;
                continue;
            } else if (arg == "--normalize") {
                enable_normalize = true;
                continue;
            }
#ifdef ENABLE_RNNOISE
            else if (arg == "--rnnoise") {
                enable_rnnoise = true;
                continue;
            } else if (arg == "--rnnoise-vad") {
                enable_rnnoise = true;
                rnnoise_vad = true;
                continue;
            }
#endif
#ifdef ENABLE_WHISPER
            else if (arg == "--transcribe-live") {
                transcribe_live = true;
                continue;
            }
#endif

            // Handle arguments with values
            if (i + 1 >= fargc)
                break;

            std::string value = fargv[i + 1];

            if (arg == "-d" || arg == "--device") {
                if (!parse_int_arg(value, arg, device_id)) {
                    return EXIT_BAD_ARGS;
                }
                ++i;
            } else if (arg == "-t" || arg == "--duration") {
                if (!parse_int_arg(value, arg, duration)) {
                    return EXIT_BAD_ARGS;
                }
                ++i;
            } else if (arg == "-o" || arg == "--output") {
                output_file = value;
                ++i;
            } else if (arg == "-f" || arg == "--format") {
                format = value;
                ++i;
            } else if (arg == "--sample-rate") {
                if (!parse_int_arg(value, arg, sample_rate)) {
                    return EXIT_BAD_ARGS;
                }
                ++i;
            } else if (arg == "--channels") {
                if (!parse_int_arg(value, arg, channels)) {
                    return EXIT_BAD_ARGS;
                }
                ++i;
            } else if (arg == "--compression") {
                if (!parse_int_arg(value, arg, compression_level)) {
                    return EXIT_BAD_ARGS;
                }
                ++i;
            } else if (arg == "--highpass") {
                if (!parse_float_arg(value, arg, highpass_freq)) {
                    return EXIT_BAD_ARGS;
                }
                enable_highpass = true;
                ++i;
            } else {
                emit_error(EXIT_BAD_ARGS, "unknown option: " + arg);
                std::cerr << "Run '" << fargv[0] << " --help' for usage.\n";
                return EXIT_BAD_ARGS;
            }
        }

        if (output_file.empty()) {
            emit_error(EXIT_BAD_ARGS, "Output file required");
            std::cerr << "Usage: " << fargv[0] << " --record -o output.wav [OPTIONS]\n";
            return EXIT_BAD_ARGS;
        }

        // Validate numeric parameters before touching the audio device
        if (sample_rate < 8000 || sample_rate > 192000) {
            emit_error(EXIT_BAD_ARGS, "--sample-rate must be between 8000 and 192000 Hz (got " +
                                          std::to_string(sample_rate) + ")");
            return EXIT_BAD_ARGS;
        }
        if (channels < 1 || channels > 2) {
            emit_error(EXIT_BAD_ARGS, "--channels must be 1 (mono) or 2 (stereo) (got " +
                                          std::to_string(channels) + ")");
            return EXIT_BAD_ARGS;
        }
        if (compression_level < 0 || compression_level > 8) {
            emit_error(EXIT_BAD_ARGS, "--compression must be between 0 and 8 (got " +
                                          std::to_string(compression_level) + ")");
            return EXIT_BAD_ARGS;
        }
        if (duration < 0) {
            emit_error(EXIT_BAD_ARGS,
                       "--duration must be >= 0 (got " + std::to_string(duration) + ")");
            return EXIT_BAD_ARGS;
        }
        if (device_id < -1) {
            emit_error(EXIT_BAD_ARGS, "--device must be >= 0, or -1 for the default device (got " +
                                          std::to_string(device_id) + ")");
            return EXIT_BAD_ARGS;
        }
        if (enable_highpass &&
            (highpass_freq <= 0.0f || highpass_freq >= static_cast<float>(sample_rate) / 2.0f)) {
            emit_error(EXIT_BAD_ARGS,
                       "--highpass frequency must be > 0 and below the Nyquist limit (" +
                           std::to_string(sample_rate / 2) + " Hz)");
            return EXIT_BAD_ARGS;
        }

        // Auto-detect format from file extension if not specified explicitly
        if (format == "wav" && output_file.size() > 5 &&
            output_file.substr(output_file.size() - 5) == ".flac") {
            format = "flac";
        }

        return record_audio(device_id, duration, output_file, sample_rate, channels, format,
                            compression_level, enable_normalize, enable_highpass, highpass_freq
#ifdef ENABLE_RNNOISE
                            ,
                            enable_rnnoise, rnnoise_vad
#endif
#ifdef ENABLE_WHISPER
                            ,
                            transcribe_live
#endif
        );
    }

    // No recognized command matched.
    emit_error(EXIT_BAD_ARGS, "unknown command or option: " + arg1);
    std::cerr << "\n";
    print_usage(fargv[0]);
    return EXIT_BAD_ARGS;
}
