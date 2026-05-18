/**
 * @file bindings.cpp
 * @brief Python bindings for ffvoice-engine using pybind11
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>

// Core ffvoice headers
#include "audio/audio_capture_device.h"
#include "audio/audio_mixer.h"
#ifdef ENABLE_RNNOISE
#include "audio/rnnoise_processor.h"
#endif
#include "audio/vad_segmenter.h"
#include "audio/whisper_processor.h"
#include "media/wav_writer.h"
#include "media/flac_writer.h"
#include "utils/ring_buffer.h"

namespace py = pybind11;
using namespace ffvoice;

PYBIND11_MODULE(_ffvoice, m) {
    m.doc() = "High-performance offline speech recognition library for Python";

    // ========== Basic Types ==========

    // Word
    py::class_<Word>(m, "Word")
        .def(py::init<>())
        .def(py::init<int64_t, int64_t, const std::string&, float>(),
             py::arg("start_ms"),
             py::arg("end_ms"),
             py::arg("text"),
             py::arg("probability"))
        .def_readonly("start_ms", &Word::start_ms, "Word start time in milliseconds")
        .def_readonly("end_ms", &Word::end_ms, "Word end time in milliseconds")
        .def_readonly("text", &Word::text, "Word text")
        .def_readonly("probability", &Word::probability,
                      "Mean token probability for this word (0.0-1.0)")
        .def("__repr__", [](const Word& w) {
            return "<Word [" + std::to_string(w.start_ms) + " -> " +
                   std::to_string(w.end_ms) + "] '" + w.text + "'>";
        });

    // TranscriptionSegment
    py::class_<TranscriptionSegment>(m, "TranscriptionSegment")
        .def(py::init<int64_t, int64_t, const std::string&, float>(),
             py::arg("start_ms"),
             py::arg("end_ms"),
             py::arg("text"),
             py::arg("confidence") = 0.0f)
        .def_readonly("start_ms", &TranscriptionSegment::start_ms,
                      "Start time in milliseconds")
        .def_readonly("end_ms", &TranscriptionSegment::end_ms,
                      "End time in milliseconds")
        .def_readonly("text", &TranscriptionSegment::text,
                      "Transcribed text")
        .def_readonly("confidence", &TranscriptionSegment::confidence,
                      "Confidence score (0.0-1.0)")
        .def_readonly("words", &TranscriptionSegment::words,
                      "Per-word timestamps (empty unless word_timestamps was enabled)")
        .def("__repr__", [](const TranscriptionSegment& seg) {
            return "<TranscriptionSegment [" + std::to_string(seg.start_ms) +
                   " -> " + std::to_string(seg.end_ms) + "] '" + seg.text + "'>";
        });

    // ========== Whisper ASR ==========

    // WhisperModelType enum
    py::enum_<WhisperModelType>(m, "WhisperModelType")
        .value("TINY", WhisperModelType::TINY, "Fastest model (~39MB, ~10x realtime)")
        .value("BASE", WhisperModelType::BASE, "Balanced speed/accuracy (~74MB, ~7x realtime)")
        .value("SMALL", WhisperModelType::SMALL, "Better accuracy (~244MB, ~3x realtime)")
        .value("MEDIUM", WhisperModelType::MEDIUM, "High accuracy (~769MB, ~1x realtime)")
        .value("LARGE", WhisperModelType::LARGE, "Best accuracy (~1550MB, <1x realtime)")
        .export_values();

    // WhisperConfig
    py::class_<WhisperConfig>(m, "WhisperConfig")
        .def(py::init<>())
        .def_readwrite("model_path", &WhisperConfig::model_path,
                       "Path to Whisper model file")
        .def_readwrite("language", &WhisperConfig::language,
                       "Language code (e.g., 'en', 'zh', 'auto')")
        .def_readwrite("model_type", &WhisperConfig::model_type,
                       "Model type (TINY, BASE, SMALL, MEDIUM, LARGE)")
        .def_readwrite("n_threads", &WhisperConfig::n_threads,
                       "Number of threads for inference")
        .def_readwrite("translate", &WhisperConfig::translate,
                       "Translate to English")
        .def_readwrite("print_progress", &WhisperConfig::print_progress,
                       "Print progress information")
        .def_readwrite("print_timestamps", &WhisperConfig::print_timestamps,
                       "Print timestamps with text")
        .def_readwrite("enable_performance_metrics", &WhisperConfig::enable_performance_metrics,
                       "Enable detailed performance metrics")
        .def_readwrite("word_timestamps", &WhisperConfig::word_timestamps,
                       "Populate per-word timestamps in each transcription segment")
        .def_readwrite("input_sample_rate", &WhisperConfig::input_sample_rate,
                       "Sample rate (Hz) of audio passed to transcribe_buffer()");

    // WhisperProcessor
    py::class_<WhisperProcessor>(m, "WhisperASR")
        .def(py::init<const WhisperConfig&>(),
             py::arg("config") = WhisperConfig(),
             "Initialize Whisper ASR processor")
        .def("initialize", &WhisperProcessor::Initialize,
             "Initialize the Whisper model")
        .def("is_initialized", &WhisperProcessor::IsInitialized,
             "Check if the model is loaded")
        .def("transcribe_file",
             [](WhisperProcessor& self, const std::string& audio_file) {
                 std::vector<TranscriptionSegment> segments;
                 bool success = self.TranscribeFile(audio_file, segments);
                 if (!success) {
                     throw std::runtime_error("Transcription failed: " + self.GetLastError());
                 }
                 return segments;
             },
             py::arg("audio_file"),
             "Transcribe an audio file and return segments")
        .def("transcribe_buffer",
             [](WhisperProcessor& self, py::array_t<int16_t> audio_array) {
                 // Get buffer info
                 py::buffer_info buf = audio_array.request();

                 // Validate buffer pointer
                 if (!buf.ptr) {
                     throw std::runtime_error("Audio array has null data pointer");
                 }

                 // Validate buffer size
                 if (buf.size == 0) {
                     throw std::runtime_error("Audio array is empty");
                 }

                 // Validate dimensions (should be 1D array)
                 if (buf.ndim != 1) {
                     throw std::runtime_error(
                         "Audio array must be 1-dimensional (got " +
                         std::to_string(buf.ndim) + " dimensions)");
                 }

                 // Get pointer and size
                 int16_t* samples = static_cast<int16_t*>(buf.ptr);
                 size_t num_samples = buf.shape[0];

                 // Transcribe
                 std::vector<TranscriptionSegment> segments;
                 bool success = self.TranscribeBuffer(samples, num_samples, segments);
                 if (!success) {
                     throw std::runtime_error("Transcription failed: " + self.GetLastError());
                 }
                 return segments;
             },
             py::arg("audio_array"),
             "Transcribe audio from NumPy array (int16, 1D) and return segments")
        .def("get_last_error", &WhisperProcessor::GetLastError,
             "Get last error message")
        .def("get_last_inference_time_ms", &WhisperProcessor::GetLastInferenceTimeMs,
             "Get last inference time in milliseconds")
        .def_static("get_model_type_name", &WhisperProcessor::GetModelTypeName,
                    py::arg("model_type"),
                    "Get model type name as string");

    // ========== Audio Device Info ==========

    // AudioDeviceInfo
    py::class_<AudioDeviceInfo>(m, "AudioDeviceInfo")
        .def_readonly("id", &AudioDeviceInfo::id, "Device ID")
        .def_readonly("name", &AudioDeviceInfo::name, "Device name")
        .def_readonly("max_input_channels", &AudioDeviceInfo::max_input_channels,
                      "Maximum input channels")
        .def_readonly("max_output_channels", &AudioDeviceInfo::max_output_channels,
                      "Maximum output channels")
        .def_readonly("supported_sample_rates", &AudioDeviceInfo::supported_sample_rates,
                      "Supported sample rates")
        .def_readonly("is_default", &AudioDeviceInfo::is_default,
                      "Is default device");

    // ========== Audio Capture Device ==========

    // AudioCaptureDevice
    py::class_<AudioCaptureDevice>(m, "AudioCapture")
        .def(py::init<>(), "Initialize audio capture device")
        .def("open", &AudioCaptureDevice::Open,
             py::arg("device_id") = -1,
             py::arg("sample_rate") = 48000,
             py::arg("channels") = 1,
             py::arg("frames_per_buffer") = 256,
             "Open audio capture device")
        .def("start",
             [](AudioCaptureDevice& self, py::function callback) {
                 // Create a persistent copy of the callback
                 // (needs to outlive the lambda since audio thread will call it)
                 auto persistent_callback = std::make_shared<py::function>(callback);

                 // Create C++ callback that wraps Python callback
                 auto cpp_callback = [persistent_callback](const int16_t* samples,
                                                            size_t num_frames) {
                     // Acquire GIL before calling Python
                     py::gil_scoped_acquire acquire;

                     try {
                         // Create NumPy array from audio data (copy)
                         py::array_t<int16_t> audio_array(num_frames);
                         auto buf = audio_array.request();
                         std::memcpy(buf.ptr, samples, num_frames * sizeof(int16_t));

                         // Call Python callback
                         (*persistent_callback)(audio_array);
                     } catch (const std::exception& e) {
                         // Log error but don't crash audio thread
                         py::print("Error in audio callback:", e.what());
                     }
                 };

                 // Start capture with C++ callback
                 return self.Start(cpp_callback);
             },
             py::arg("callback"),
             "Start capturing audio with Python callback (receives NumPy array)")
        .def("stop", &AudioCaptureDevice::Stop,
             "Stop capturing audio")
        .def("close", &AudioCaptureDevice::Close,
             "Close the device")
        .def("is_open", &AudioCaptureDevice::IsOpen,
             "Check if device is open")
        .def("is_capturing", &AudioCaptureDevice::IsCapturing,
             "Check if currently capturing")
        .def("get_sample_rate", &AudioCaptureDevice::GetSampleRate,
             "Get current sample rate")
        .def("get_channels", &AudioCaptureDevice::GetChannels,
             "Get current channel count")
        .def_static("initialize", &AudioCaptureDevice::Initialize,
                    "Initialize PortAudio")
        .def_static("terminate", &AudioCaptureDevice::Terminate,
                    "Terminate PortAudio")
        .def_static("get_devices", &AudioCaptureDevice::GetDevices,
                    "Get list of available audio devices")
        .def_static("get_default_input_device", &AudioCaptureDevice::GetDefaultInputDevice,
                    "Get default input device ID");

#ifdef ENABLE_RNNOISE
    // ========== RNNoise Processor ==========

    // RNNoiseConfig
    py::class_<RNNoiseConfig>(m, "RNNoiseConfig")
        .def(py::init<>())
        .def_readwrite("enable_vad", &RNNoiseConfig::enable_vad,
                       "Enable VAD (Voice Activity Detection)");

    // RNNoiseProcessor
    py::class_<RNNoiseProcessor>(m, "RNNoise")
        .def(py::init<const RNNoiseConfig&>(),
             py::arg("config") = RNNoiseConfig(),
             "Initialize RNNoise noise suppressor")
        .def("initialize", &RNNoiseProcessor::Initialize,
             py::arg("sample_rate"),
             py::arg("channels"),
             "Initialize with sample rate and channels")
        .def("process",
             [](RNNoiseProcessor& self, py::array_t<int16_t> audio_array) {
                 // Get buffer info
                 py::buffer_info buf = audio_array.request();

                 // Validate buffer pointer
                 if (!buf.ptr) {
                     throw std::runtime_error("Audio array has null data pointer");
                 }

                 // Validate buffer size
                 if (buf.size == 0) {
                     throw std::runtime_error("Audio array is empty");
                 }

                 // Validate dimensions (should be 1D array)
                 if (buf.ndim != 1) {
                     throw std::runtime_error(
                         "Audio array must be 1-dimensional (got " +
                         std::to_string(buf.ndim) + " dimensions)");
                 }

                 // Check if array is writable
                 if (buf.readonly) {
                     throw std::runtime_error("Audio array must be writable");
                 }

                 // Get pointer and size
                 int16_t* samples = static_cast<int16_t*>(buf.ptr);
                 size_t num_samples = buf.shape[0];

                 // Process audio (in-place)
                 self.Process(samples, num_samples);
             },
             py::arg("audio_array"),
             "Process audio with RNNoise (in-place modification of NumPy array)")
        .def("reset", &RNNoiseProcessor::Reset,
             "Reset internal state")
        .def("get_vad_probability", &RNNoiseProcessor::GetVADProbability,
             "Get last VAD probability (0.0-1.0)");
#endif  // ENABLE_RNNOISE

    // ========== VAD Segmenter ==========

    // VAD Sensitivity enum
    py::enum_<VADSegmenter::Sensitivity>(m, "VADSensitivity")
        .value("VERY_SENSITIVE", VADSegmenter::Sensitivity::VERY_SENSITIVE,
               "Detect very quiet speech (threshold=0.3)")
        .value("SENSITIVE", VADSegmenter::Sensitivity::SENSITIVE,
               "Detect quiet speech (threshold=0.4)")
        .value("BALANCED", VADSegmenter::Sensitivity::BALANCED,
               "Balanced detection (threshold=0.5, default)")
        .value("CONSERVATIVE", VADSegmenter::Sensitivity::CONSERVATIVE,
               "Avoid false positives (threshold=0.6)")
        .value("VERY_CONSERVATIVE", VADSegmenter::Sensitivity::VERY_CONSERVATIVE,
               "Only very clear speech (threshold=0.7)")
        .export_values();

    // VADSegmenter::Config
    py::class_<VADSegmenter::Config>(m, "VADConfig")
        .def(py::init<>())
        .def_readwrite("speech_threshold", &VADSegmenter::Config::speech_threshold,
                       "VAD probability threshold for speech detection")
        .def_readwrite("min_speech_frames", &VADSegmenter::Config::min_speech_frames,
                       "Minimum consecutive frames to trigger speech start")
        .def_readwrite("min_silence_frames", &VADSegmenter::Config::min_silence_frames,
                       "Minimum consecutive frames to trigger speech end")
        .def_readwrite("max_segment_samples", &VADSegmenter::Config::max_segment_samples,
                       "Maximum segment length in samples")
        .def_readwrite("enable_adaptive_threshold", &VADSegmenter::Config::enable_adaptive_threshold,
                       "Enable adaptive threshold adjustment")
        .def_readwrite("adaptive_factor", &VADSegmenter::Config::adaptive_factor,
                       "Adaptation speed (0.0-1.0)")
        .def_static("from_preset", &VADSegmenter::Config::FromPreset,
                    py::arg("sensitivity"),
                    "Create config from sensitivity preset");

    // VADSegmenter
    py::class_<VADSegmenter>(m, "VADSegmenter")
        .def(py::init<const VADSegmenter::Config&>(),
             py::arg("config") = VADSegmenter::Config(),
             "Initialize VAD segmenter with configuration")
        .def("process_frame",
             [](VADSegmenter& self, py::array_t<int16_t> audio_array, float vad_prob,
                py::function callback) {
                 // Get buffer info
                 py::buffer_info buf = audio_array.request();

                 // Validate dimensions
                 if (buf.ndim != 1) {
                     throw std::runtime_error(
                         "Audio array must be 1-dimensional (got " +
                         std::to_string(buf.ndim) + " dimensions)");
                 }

                 // Get pointer and size
                 const int16_t* samples = static_cast<const int16_t*>(buf.ptr);
                 size_t num_samples = buf.shape[0];

                 // Create C++ callback that wraps Python callback
                 auto cpp_callback = [callback](const int16_t* seg_samples, size_t seg_size) {
                     // Acquire GIL before calling Python
                     py::gil_scoped_acquire acquire;

                     // Create NumPy array from segment data (copy)
                     py::array_t<int16_t> segment_array(seg_size);
                     auto seg_buf = segment_array.request();
                     std::memcpy(seg_buf.ptr, seg_samples, seg_size * sizeof(int16_t));

                     // Call Python callback
                     callback(segment_array);
                 };

                 // Process frame with C++ callback
                 // Release GIL during C++ processing
                 py::gil_scoped_release release;
                 self.ProcessFrame(samples, num_samples, vad_prob, cpp_callback);
             },
             py::arg("audio_array"),
             py::arg("vad_prob"),
             py::arg("callback"),
             "Process audio frame with VAD probability and callback")
        .def("flush",
             [](VADSegmenter& self, py::function callback) {
                 // Create C++ callback that wraps Python callback
                 auto cpp_callback = [callback](const int16_t* seg_samples, size_t seg_size) {
                     // Acquire GIL before calling Python
                     py::gil_scoped_acquire acquire;

                     // Create NumPy array from segment data (copy)
                     py::array_t<int16_t> segment_array(seg_size);
                     auto seg_buf = segment_array.request();
                     std::memcpy(seg_buf.ptr, seg_samples, seg_size * sizeof(int16_t));

                     // Call Python callback
                     callback(segment_array);
                 };

                 // Flush with C++ callback
                 // Release GIL during C++ processing
                 py::gil_scoped_release release;
                 self.Flush(cpp_callback);
             },
             py::arg("callback"),
             "Flush remaining buffered audio with callback")
        .def("reset", &VADSegmenter::Reset,
             "Reset segmenter state")
        .def("get_buffer_size", &VADSegmenter::GetBufferSize,
             "Get current buffer size in samples")
        .def("is_in_speech", &VADSegmenter::IsInSpeech,
             "Check if currently in a speech segment")
        .def("get_current_threshold", &VADSegmenter::GetCurrentThreshold,
             "Get current adaptive threshold value")
        .def("get_statistics",
             [](VADSegmenter& self) {
                 float avg_vad_prob = 0.0f;
                 float speech_ratio = 0.0f;
                 self.GetStatistics(avg_vad_prob, speech_ratio);
                 return py::make_tuple(avg_vad_prob, speech_ratio);
             },
             "Get VAD statistics: (avg_vad_prob, speech_ratio)");

    // ========== Audio Writers ==========

    // WAVWriter
    py::class_<WavWriter>(m, "WAVWriter")
        .def(py::init<>(), "Create WAV file writer")
        .def("open", &WavWriter::Open,
             py::arg("filename"),
             py::arg("sample_rate"),
             py::arg("channels"),
             py::arg("bits_per_sample") = 16,
             "Open WAV file for writing")
        .def("write_samples",
             py::overload_cast<const std::vector<int16_t>&>(&WavWriter::WriteSamples),
             py::arg("samples"),
             "Write PCM samples from vector")
        .def("write_samples_array",
             [](WavWriter& self, py::array_t<int16_t> audio_array) {
                 // Get buffer info
                 py::buffer_info buf = audio_array.request();

                 // Validate dimensions (should be 1D array)
                 if (buf.ndim != 1) {
                     throw std::runtime_error(
                         "Audio array must be 1-dimensional (got " +
                         std::to_string(buf.ndim) + " dimensions)");
                 }

                 // Get pointer and size
                 const int16_t* samples = static_cast<const int16_t*>(buf.ptr);
                 size_t num_samples = buf.shape[0];

                 // Write samples
                 return self.WriteSamples(samples, num_samples);
             },
             py::arg("audio_array"),
             "Write PCM samples from NumPy array (int16, 1D)")
        .def("is_open", &WavWriter::IsOpen,
             "Check if file is open")
        .def("get_total_samples", &WavWriter::GetTotalSamples,
             "Get total samples written")
        .def("close", &WavWriter::Close,
             "Close the WAV file");

    // FLACWriter
    py::class_<FlacWriter>(m, "FLACWriter")
        .def(py::init<>(), "Create FLAC file writer")
        .def("open", &FlacWriter::Open,
             py::arg("filename"),
             py::arg("sample_rate"),
             py::arg("channels"),
             py::arg("bits_per_sample") = 16,
             py::arg("compression_level") = 5,
             "Open FLAC file for writing")
        .def("write_samples",
             py::overload_cast<const std::vector<int16_t>&>(&FlacWriter::WriteSamples),
             py::arg("samples"),
             "Write PCM samples from vector")
        .def("write_samples_array",
             [](FlacWriter& self, py::array_t<int16_t> audio_array) {
                 // Get buffer info
                 py::buffer_info buf = audio_array.request();

                 // Validate dimensions (should be 1D array)
                 if (buf.ndim != 1) {
                     throw std::runtime_error(
                         "Audio array must be 1-dimensional (got " +
                         std::to_string(buf.ndim) + " dimensions)");
                 }

                 // Get pointer and size
                 const int16_t* samples = static_cast<const int16_t*>(buf.ptr);
                 size_t num_samples = buf.shape[0];

                 // Write samples
                 return self.WriteSamples(samples, num_samples);
             },
             py::arg("audio_array"),
             "Write PCM samples from NumPy array (int16, 1D)")
        .def("is_open", &FlacWriter::IsOpen,
             "Check if file is open")
        .def("get_total_samples", &FlacWriter::GetTotalSamples,
             "Get total samples written")
        .def("get_compression_ratio", &FlacWriter::GetCompressionRatio,
             "Get compression ratio (original_size / compressed_size)")
        .def("close", &FlacWriter::Close,
             "Close the FLAC file");

    // ========== Audio Mixer ==========

    // AudioMixer
    py::class_<AudioMixer>(m, "AudioMixer")
        .def(py::init<>(), "Create a multi-track audio mixer")
        .def("initialize", &AudioMixer::Initialize,
             py::arg("sample_rate"),
             py::arg("channels"),
             "Initialize the mixer (channels must be 1 or 2)")
        .def("is_initialized", &AudioMixer::IsInitialized,
             "Check if the mixer is initialized")
        .def("get_sample_rate", &AudioMixer::GetSampleRate, "Get the sample rate")
        .def("get_channels", &AudioMixer::GetChannels, "Get the channel count")
        .def("add_track", &AudioMixer::AddTrack,
             py::arg("gain") = 1.0f,
             py::arg("pan") = 0.0f,
             "Add a track; returns its track id (-1 if the mixer is not initialized)")
        .def("remove_track", &AudioMixer::RemoveTrack, py::arg("track_id"),
             "Remove a track; returns True if it existed")
        .def("has_track", &AudioMixer::HasTrack, py::arg("track_id"),
             "Check whether a track exists")
        .def("get_track_count", &AudioMixer::GetTrackCount, "Number of registered tracks")
        .def("set_gain", &AudioMixer::SetGain, py::arg("track_id"), py::arg("gain"),
             "Set a track's linear gain (clamped to [0, 8])")
        .def("set_pan", &AudioMixer::SetPan, py::arg("track_id"), py::arg("pan"),
             "Set a track's stereo pan (-1 = left, 0 = center, +1 = right)")
        .def("set_mute", &AudioMixer::SetMute, py::arg("track_id"), py::arg("muted"),
             "Mute or unmute a track")
        .def("get_gain", &AudioMixer::GetGain, py::arg("track_id"), "Get a track's gain")
        .def("get_pan", &AudioMixer::GetPan, py::arg("track_id"), "Get a track's pan")
        .def("is_muted", &AudioMixer::IsMuted, py::arg("track_id"), "Check if a track is muted")
        .def("set_master_gain", &AudioMixer::SetMasterGain, py::arg("gain"),
             "Set the master gain applied to the mix (clamped to [0, 8])")
        .def("get_master_gain", &AudioMixer::GetMasterGain, "Get the master gain")
        .def("reset", &AudioMixer::Reset, "Remove all tracks and reset the master gain")
        .def("mix_block",
             [](AudioMixer& self, const py::dict& tracks) {
                 // tracks: { track_id (int) : 1-D int16 NumPy array }.
                 // Every array must have the same length.
                 std::vector<py::array_t<int16_t>> arrays;  // keep buffers alive
                 std::vector<MixerInput> inputs;
                 size_t num_samples = 0;
                 bool first = true;

                 for (auto item : tracks) {
                     int track_id = item.first.cast<int>();
                     auto arr = item.second.cast<py::array_t<int16_t>>();
                     py::buffer_info buf = arr.request();
                     if (buf.ndim != 1) {
                         throw std::runtime_error("Each track array must be 1-dimensional");
                     }
                     size_t n = static_cast<size_t>(buf.shape[0]);
                     if (first) {
                         num_samples = n;
                         first = false;
                     } else if (n != num_samples) {
                         throw std::runtime_error("All track arrays must have the same length");
                     }
                     arrays.push_back(arr);
                     inputs.push_back(MixerInput{track_id, static_cast<const int16_t*>(buf.ptr)});
                 }

                 py::array_t<int16_t> output(static_cast<py::ssize_t>(num_samples));
                 py::buffer_info out_buf = output.request();
                 if (!self.MixBlock(inputs, static_cast<int16_t*>(out_buf.ptr), num_samples)) {
                     throw std::runtime_error(
                         "mix_block failed (mixer not initialized or invalid arguments)");
                 }
                 return output;
             },
             py::arg("tracks"),
             "Mix {track_id: int16 ndarray} into a single mixed int16 ndarray");

    // ========== Ring Buffer ==========

    // RingBuffer<int16_t> - lock-free SPSC ring buffer for audio samples
    py::class_<RingBuffer<int16_t>>(m, "RingBuffer")
        .def(py::init<size_t>(), py::arg("capacity"),
             "Create a lock-free SPSC ring buffer for up to `capacity` int16 samples")
        .def("capacity", &RingBuffer<int16_t>::capacity, "Maximum number of elements")
        .def("size", &RingBuffer<int16_t>::size, "Number of elements currently stored")
        .def("empty", &RingBuffer<int16_t>::empty, "True if the buffer is empty")
        .def("full", &RingBuffer<int16_t>::full, "True if the buffer is full")
        .def("available_read", &RingBuffer<int16_t>::available_read,
             "Number of elements available to pop")
        .def("available_write", &RingBuffer<int16_t>::available_write,
             "Number of elements that can still be pushed")
        .def("push", &RingBuffer<int16_t>::push, py::arg("value"),
             "Push one value; returns False if the buffer is full")
        .def("pop",
             [](RingBuffer<int16_t>& self) -> py::object {
                 int16_t value = 0;
                 if (self.pop(value)) {
                     return py::cast(value);
                 }
                 return py::none();
             },
             "Pop one value; returns None if the buffer is empty")
        .def("push_bulk",
             [](RingBuffer<int16_t>& self, py::array_t<int16_t> data) {
                 py::buffer_info buf = data.request();
                 if (buf.ndim != 1) {
                     throw std::runtime_error("data must be a 1-dimensional int16 array");
                 }
                 return self.push_bulk(static_cast<const int16_t*>(buf.ptr),
                                       static_cast<size_t>(buf.shape[0]));
             },
             py::arg("data"),
             "Push values from a 1-D int16 ndarray; returns the number actually pushed")
        .def("pop_bulk",
             [](RingBuffer<int16_t>& self, size_t count) {
                 std::vector<int16_t> tmp(count);
                 size_t n = self.pop_bulk(tmp.data(), count);
                 return py::array_t<int16_t>(static_cast<py::ssize_t>(n), tmp.data());
             },
             py::arg("count"),
             "Pop up to `count` values; returns them as a 1-D int16 ndarray")
        .def("clear", &RingBuffer<int16_t>::clear, "Reset the buffer to empty");
}
