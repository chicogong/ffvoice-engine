/**
 * @file audio_converter.cpp
 * @brief Implementation of audio format conversion utilities
 */

#include "utils/audio_converter.h"

#include "utils/logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

// FLAC decoder
#include <FLAC/stream_decoder.h>

namespace ffvoice {

// ============================================================================
// Public Methods
// ============================================================================

bool AudioConverter::LoadAndConvert(const std::string& filename, std::vector<float>& pcm_data,
                                    int target_sample_rate) {
    // Determine file type by extension
    std::string ext;
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        ext = filename.substr(dot_pos);
        // Convert to lowercase
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    // Load audio file
    std::vector<float> raw_pcm;
    int sample_rate = 0;
    int channels = 0;

    bool success = false;
    if (ext == ".wav") {
        success = LoadWAV(filename, raw_pcm, sample_rate, channels);
    } else if (ext == ".flac") {
        success = LoadFLAC(filename, raw_pcm, sample_rate, channels);
    } else {
        LOG_ERROR("Unsupported audio file format: %s", ext.c_str());
        return false;
    }

    if (!success) {
        LOG_ERROR("Failed to load audio file: %s", filename.c_str());
        return false;
    }

    LOG_INFO("Loaded audio: %d Hz, %d channels, %zu samples", sample_rate, channels,
             raw_pcm.size());

    // Convert stereo to mono if needed
    std::vector<float> mono_pcm;
    if (channels == 2) {
        size_t num_frames = raw_pcm.size() / 2;
        mono_pcm.resize(num_frames);
        StereoToMono(raw_pcm.data(), num_frames, mono_pcm.data());
        LOG_INFO("Converted stereo to mono: %zu frames", num_frames);
    } else if (channels == 1) {
        mono_pcm = std::move(raw_pcm);
    } else {
        LOG_ERROR("Unsupported channel count: %d", channels);
        return false;
    }

    // Resample if needed
    if (sample_rate != target_sample_rate) {
        size_t output_size = static_cast<size_t>(
            mono_pcm.size() * static_cast<double>(target_sample_rate) / sample_rate);
        pcm_data.resize(output_size);
        Resample(mono_pcm.data(), mono_pcm.size(), sample_rate, pcm_data.data(), pcm_data.size(),
                 target_sample_rate);
        LOG_INFO("Resampled: %d Hz → %d Hz (%zu samples)", sample_rate, target_sample_rate,
                 pcm_data.size());
    } else {
        pcm_data = std::move(mono_pcm);
    }

    return true;
}

void AudioConverter::Int16ToFloat(const int16_t* input, size_t num_samples, float* output) {
    for (size_t i = 0; i < num_samples; ++i) {
        output[i] = input[i] / 32768.0f;
    }
}

void AudioConverter::FloatToInt16(const float* input, size_t num_samples, int16_t* output) {
    for (size_t i = 0; i < num_samples; ++i) {
        float clamped = std::clamp(input[i], -1.0f, 1.0f);
        output[i] = static_cast<int16_t>(clamped * 32767.0f);
    }
}

void AudioConverter::Resample(const float* input, size_t input_size, int input_rate, float* output,
                              size_t output_size, int output_rate) {
    if (input_size == 0 || output_size == 0) {
        return;
    }

    // Linear interpolation resampling
    double ratio = static_cast<double>(input_rate) / output_rate;

    for (size_t i = 0; i < output_size; ++i) {
        double src_pos = i * ratio;
        size_t src_index = static_cast<size_t>(src_pos);

        if (src_index >= input_size - 1) {
            // Last sample
            output[i] = input[input_size - 1];
        } else {
            // Linear interpolation between src_index and src_index + 1
            double frac = src_pos - src_index;
            output[i] = input[src_index] * (1.0 - frac) + input[src_index + 1] * frac;
        }
    }
}

void AudioConverter::StereoToMono(const float* stereo, size_t num_frames, float* mono) {
    for (size_t i = 0; i < num_frames; ++i) {
        // Average left and right channels
        mono[i] = (stereo[i * 2] + stereo[i * 2 + 1]) * 0.5f;
    }
}

// ============================================================================
// Private Methods - WAV Loading
// ============================================================================

// WAV file format structures
#pragma pack(push, 1)
struct WavHeader {
    char riff[4];        // "RIFF"
    uint32_t file_size;  // File size - 8
    char wave[4];        // "WAVE"
};

struct WavChunkHeader {
    char id[4];     // Chunk ID
    uint32_t size;  // Chunk size
};

struct WavFormatChunk {
    uint16_t format;           // Audio format (1 = PCM)
    uint16_t channels;         // Number of channels
    uint32_t sample_rate;      // Sample rate
    uint32_t byte_rate;        // Byte rate
    uint16_t block_align;      // Block align
    uint16_t bits_per_sample;  // Bits per sample
};
#pragma pack(pop)

bool AudioConverter::LoadWAV(const std::string& filename, std::vector<float>& pcm_data,
                             int& sample_rate, int& channels) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open WAV file: %s", filename.c_str());
        return false;
    }

    // Read RIFF header
    WavHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (std::memcmp(header.riff, "RIFF", 4) != 0 || std::memcmp(header.wave, "WAVE", 4) != 0) {
        LOG_ERROR("Invalid WAV file format");
        return false;
    }

    // Find format chunk
    WavFormatChunk fmt;
    bool found_fmt = false;

    while (file.good()) {
        WavChunkHeader chunk;
        file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));

        if (!file.good())
            break;

        if (std::memcmp(chunk.id, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&fmt), sizeof(fmt));
            found_fmt = true;

            // Skip any extra format bytes
            if (chunk.size > sizeof(fmt)) {
                file.seekg(chunk.size - sizeof(fmt), std::ios::cur);
            }
        } else if (std::memcmp(chunk.id, "data", 4) == 0 && found_fmt) {
            // Found data chunk - read audio data
            sample_rate = fmt.sample_rate;
            channels = fmt.channels;

            if (fmt.format != 1) {  // Only support PCM
                LOG_ERROR("Unsupported WAV format: %d (only PCM supported)", fmt.format);
                return false;
            }

            if (fmt.bits_per_sample != 16) {
                LOG_ERROR("Unsupported bit depth: %d (only 16-bit supported)", fmt.bits_per_sample);
                return false;
            }

            // Read int16 samples
            size_t num_samples = chunk.size / sizeof(int16_t);
            std::vector<int16_t> int16_data(num_samples);
            file.read(reinterpret_cast<char*>(int16_data.data()), chunk.size);

            // Convert to float
            pcm_data.resize(num_samples);
            Int16ToFloat(int16_data.data(), num_samples, pcm_data.data());

            return true;
        } else {
            // Skip unknown chunk
            file.seekg(chunk.size, std::ios::cur);
        }
    }

    LOG_ERROR("No data chunk found in WAV file");
    return false;
}

// ============================================================================
// Private Methods - FLAC Loading
// ============================================================================

// FLAC decoder callback data
struct FLACDecoderData {
    std::vector<float>* pcm_data;
    int sample_rate;
    int channels;
    int bits_per_sample;
};

// FLAC write callback
static FLAC__StreamDecoderWriteStatus flac_write_callback(const FLAC__StreamDecoder* /*decoder*/,
                                                          const FLAC__Frame* frame,
                                                          const FLAC__int32* const buffer[],
                                                          void* client_data) {
    FLACDecoderData* data = static_cast<FLACDecoderData*>(client_data);

    // Get number of samples in this frame
    unsigned block_size = frame->header.blocksize;
    unsigned channels = frame->header.channels;

    // Calculate conversion factor based on bit depth
    float scale = 1.0f / (1 << (data->bits_per_sample - 1));

    // Append samples to output (interleaved)
    size_t old_size = data->pcm_data->size();
    data->pcm_data->resize(old_size + block_size * channels);

    for (unsigned i = 0; i < block_size; ++i) {
        for (unsigned ch = 0; ch < channels; ++ch) {
            (*data->pcm_data)[old_size + i * channels + ch] = buffer[ch][i] * scale;
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

// FLAC metadata callback
static void flac_metadata_callback(const FLAC__StreamDecoder* /*decoder*/,
                                   const FLAC__StreamMetadata* metadata, void* client_data) {
    FLACDecoderData* data = static_cast<FLACDecoderData*>(client_data);

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        data->sample_rate = metadata->data.stream_info.sample_rate;
        data->channels = metadata->data.stream_info.channels;
        data->bits_per_sample = metadata->data.stream_info.bits_per_sample;

        LOG_INFO("FLAC metadata: %d Hz, %d channels, %d bits", data->sample_rate, data->channels,
                 data->bits_per_sample);
    }
}

// FLAC error callback
static void flac_error_callback(const FLAC__StreamDecoder* /*decoder*/,
                                FLAC__StreamDecoderErrorStatus status, void* /*client_data*/) {
    LOG_ERROR("FLAC decoder error: %s", FLAC__StreamDecoderErrorStatusString[status]);
}

bool AudioConverter::LoadFLAC(const std::string& filename, std::vector<float>& pcm_data,
                              int& sample_rate, int& channels) {
    // Create FLAC decoder
    FLAC__StreamDecoder* decoder = FLAC__stream_decoder_new();
    if (!decoder) {
        LOG_ERROR("Failed to create FLAC decoder");
        return false;
    }

    // Setup decoder data
    FLACDecoderData data;
    data.pcm_data = &pcm_data;
    data.sample_rate = 0;
    data.channels = 0;
    data.bits_per_sample = 0;

    // Initialize decoder
    FLAC__StreamDecoderInitStatus init_status =
        FLAC__stream_decoder_init_file(decoder, filename.c_str(), flac_write_callback,
                                       flac_metadata_callback, flac_error_callback, &data);

    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        LOG_ERROR("Failed to initialize FLAC decoder: %s",
                  FLAC__StreamDecoderInitStatusString[init_status]);
        FLAC__stream_decoder_delete(decoder);
        return false;
    }

    // Process entire file
    bool success = FLAC__stream_decoder_process_until_end_of_stream(decoder);

    // Get output parameters
    sample_rate = data.sample_rate;
    channels = data.channels;

    // Cleanup
    FLAC__stream_decoder_finish(decoder);
    FLAC__stream_decoder_delete(decoder);

    if (!success) {
        LOG_ERROR("FLAC decoding failed");
        return false;
    }

    return true;
}

}  // namespace ffvoice
