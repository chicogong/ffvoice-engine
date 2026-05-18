/**
 * @file test_live_captioner.cpp
 * @brief Unit tests for LiveCaptioner
 * @note Only compiled when ENABLE_WHISPER is defined
 */

#ifdef ENABLE_WHISPER

    #include "audio/live_captioner.h"

    #include <gtest/gtest.h>

    #include <atomic>
    #include <chrono>
    #include <mutex>
    #include <thread>
    #include <vector>

using namespace ffvoice;

// =============================================================================
// Helper utilities
// =============================================================================

namespace {

/// Build a minimal LiveCaptionerConfig with a mock transcribe_fn so no real
/// Whisper model is required.
LiveCaptionerConfig MakeTestConfig(
    std::function<bool(const int16_t*, size_t, std::vector<TranscriptionSegment>&)> fn = nullptr) {
    LiveCaptionerConfig cfg;

    // VAD tuned for fast unit-test triggering
    cfg.vad.speech_threshold = 0.5f;
    cfg.vad.min_speech_frames = 2;   // triggers after ~2 batches of speech
    cfg.vad.min_silence_frames = 2;  // triggers after ~2 batches of silence

    cfg.partial_interval_ms = 200;
    cfg.min_samples_for_partial = 8000;  // ~167 ms at 48 kHz
    cfg.ring_buffer_capacity = 144000;   // 3 s at 48 kHz
    cfg.suppress_whisper_progress = true;

    // Provide a fixed VAD probability source driven by sample amplitude.
    // We set it to nullptr here; individual tests override it.
    cfg.vad_prob_source = nullptr;

    // Default mock: returns one segment with known text and confidence.
    if (fn) {
        cfg.transcribe_fn = std::move(fn);
    } else {
        cfg.transcribe_fn = [](const int16_t* /*samples*/, size_t /*count*/,
                               std::vector<TranscriptionSegment>& out) -> bool {
            out.clear();
            out.emplace_back(0LL, 500LL, "hello world", 0.9f);
            return true;
        };
    }

    return cfg;
}

/// Generate silent (zero) samples.
std::vector<int16_t> MakeSilence(size_t count) {
    return std::vector<int16_t>(count, 0);
}

/// Generate loud speech-like samples (amplitude 10000).
std::vector<int16_t> MakeSpeech(size_t count) {
    return std::vector<int16_t>(count, 10000);
}

/// Wait up to @p timeout_ms for @p cond to return true, polling every 5 ms.
/// Returns true if condition was met within the timeout.
bool WaitFor(std::function<bool()> cond, int timeout_ms = 3000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (cond()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

}  // anonymous namespace

// =============================================================================
// Test fixture
// =============================================================================

class LiveCaptionerTest : public ::testing::Test {
protected:
    // Collected events, protected by events_mutex_
    std::vector<CaptionEvent> events_;
    std::mutex events_mutex_;

    CaptionCallback MakeCallback() {
        return [this](const CaptionEvent& ev) {
            std::lock_guard<std::mutex> lock(events_mutex_);
            events_.push_back(ev);
        };
    }

    size_t EventCount() {
        std::lock_guard<std::mutex> lock(events_mutex_);
        return events_.size();
    }

    std::vector<CaptionEvent> CollectEvents() {
        std::lock_guard<std::mutex> lock(events_mutex_);
        return events_;
    }

    void SetUp() override {
        events_.clear();
    }
};

// =============================================================================
// Construction and initialization tests
// =============================================================================

TEST_F(LiveCaptionerTest, DefaultConstruction_DoesNotCrash) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    LiveCaptioner captioner(cfg);

    EXPECT_FALSE(captioner.IsRunning());
    EXPECT_TRUE(captioner.GetLastError().empty());
}

TEST_F(LiveCaptionerTest, Initialize_WithTestSeam_Succeeds) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    LiveCaptioner captioner(cfg);

    EXPECT_TRUE(captioner.Initialize());
    EXPECT_TRUE(captioner.GetLastError().empty());
}

TEST_F(LiveCaptionerTest, DoubleInitialize_ReturnsTrueAndDoesNotCrash) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    LiveCaptioner captioner(cfg);

    EXPECT_TRUE(captioner.Initialize());
    EXPECT_TRUE(captioner.Initialize());  // second call is a no-op
}

// =============================================================================
// Start / Stop lifecycle tests
// =============================================================================

TEST_F(LiveCaptionerTest, StartStop_BasicLifecycle) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());

    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());
    EXPECT_TRUE(captioner.IsRunning());

    captioner.Stop();
    EXPECT_FALSE(captioner.IsRunning());
}

TEST_F(LiveCaptionerTest, StartBeforeInitialize_ReturnsFalse) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    LiveCaptioner captioner(cfg);

    EXPECT_FALSE(captioner.Start());
    EXPECT_FALSE(captioner.IsRunning());
    EXPECT_FALSE(captioner.GetLastError().empty());
}

TEST_F(LiveCaptionerTest, DoubleStart_ReturnsTrueAndDoesNotSpawnExtraThread) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    EXPECT_TRUE(captioner.Start());  // second call is a no-op

    captioner.Stop();
}

TEST_F(LiveCaptionerTest, MultipleStop_DoesNotCrash) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    captioner.Start();
    captioner.Stop();
    captioner.Stop();  // second stop should be safe
}

// =============================================================================
// FeedAudio tests
// =============================================================================

TEST_F(LiveCaptionerTest, FeedAudio_EmptyFeed_NoEventNocrash) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    captioner.Start();

    // Feed nothing
    size_t written = captioner.FeedAudio(nullptr, 0);
    EXPECT_EQ(0u, written);

    // Feed empty vector via valid pointer but count=0
    std::vector<int16_t> empty;
    empty.resize(0);
    written = captioner.FeedAudio(empty.data(), 0);
    EXPECT_EQ(0u, written);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    captioner.Stop();

    EXPECT_EQ(0u, EventCount());
}

TEST_F(LiveCaptionerTest, FeedAudio_ReturnCount_NoMoreThanRequested) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    captioner.Start();

    auto samples = MakeSilence(4800);
    size_t written = captioner.FeedAudio(samples.data(), samples.size());

    EXPECT_LE(written, samples.size());
    EXPECT_GT(written, 0u);

    captioner.Stop();
}

TEST_F(LiveCaptionerTest, FeedAudio_RingBufferFull_ReturnsShortCount) {
    // Use a tiny ring buffer (smaller than what we'll try to push)
    LiveCaptionerConfig cfg = MakeTestConfig();
    cfg.ring_buffer_capacity = 1000;  // very small — 1000 samples
    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    // Don't start worker so the buffer stays full

    // First fill: should succeed fully
    auto batch1 = MakeSilence(1000);
    size_t w1 = captioner.FeedAudio(batch1.data(), batch1.size());
    EXPECT_EQ(1000u, w1);

    // Second push: ring is full, should return 0 (or short)
    auto batch2 = MakeSilence(1000);
    size_t w2 = captioner.FeedAudio(batch2.data(), batch2.size());
    EXPECT_LT(w2, 1000u);  // must be short-written
}

// =============================================================================
// Final event tests
// =============================================================================

TEST_F(LiveCaptionerTest, Final_FiresAfterSpeechThenSilence) {
    LiveCaptionerConfig cfg = MakeTestConfig();

    // Use the built-in RMS estimator (vad_prob_source=nullptr).
    // MakeSpeech(4800) produces amplitude 10000 → RMS=10000 → vad_prob=1.0.
    // MakeSilence(4800) produces amplitude 0    → RMS=0    → vad_prob=0.0.
    // This guarantees each worker batch sees the correct VAD probability
    // derived from the sample data itself, avoiding any ordering race.
    cfg.vad_prob_source = nullptr;

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    // --- Speech phase: amplitude 10000 → RMS-based vad_prob clamped to 1.0 ---
    // Feed enough to exceed min_speech_frames (2 worker batches of 4800 samples)
    for (int i = 0; i < 4; ++i) {
        auto speech = MakeSpeech(4800);
        captioner.FeedAudio(speech.data(), speech.size());
    }

    // --- Silence phase: amplitude 0 → vad_prob=0.0 ---
    // Feed enough to exceed min_silence_frames (2 worker batches)
    for (int i = 0; i < 4; ++i) {
        auto silence = MakeSilence(4800);
        captioner.FeedAudio(silence.data(), silence.size());
    }

    // Wait for a Final event (generous timeout for worker thread + sleeps)
    bool got_final = WaitFor(
        [this]() {
            auto evs = CollectEvents();
            for (const auto& ev : evs) {
                if (ev.type == CaptionEventType::Final) {
                    return true;
                }
            }
            return false;
        },
        4000);

    captioner.Stop();

    EXPECT_TRUE(got_final) << "Expected at least one Final event after speech+silence";

    auto evs = CollectEvents();
    for (const auto& ev : evs) {
        if (ev.type == CaptionEventType::Final) {
            EXPECT_EQ("hello world", ev.text);
            EXPECT_NEAR(0.9f, ev.confidence, 0.01f);
        }
    }
}

TEST_F(LiveCaptionerTest, Stop_FlushesBufferedFinal) {
    LiveCaptionerConfig cfg = MakeTestConfig();

    // Constant high VAD probability so speech never naturally ends
    cfg.vad_prob_source = []() { return 0.9f; };
    // Very long silence requirement so VAD won't self-trigger during the test
    cfg.vad.min_silence_frames = 10000;

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    // Feed enough speech to accumulate
    for (int i = 0; i < 6; ++i) {
        auto speech = MakeSpeech(4800);
        captioner.FeedAudio(speech.data(), speech.size());
    }

    // Give the worker time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Stop should flush a Final even though silence was never detected
    captioner.Stop();

    auto evs = CollectEvents();
    bool has_final = false;
    for (const auto& ev : evs) {
        if (ev.type == CaptionEventType::Final) {
            has_final = true;
        }
    }
    EXPECT_TRUE(has_final) << "Stop() must flush a pending Final event";
}

// =============================================================================
// Utterance ID tests
// =============================================================================

TEST_F(LiveCaptionerTest, UtteranceId_IncrementsPerFinal) {
    LiveCaptionerConfig cfg = MakeTestConfig();

    // Use the built-in RMS estimator so VAD probability tracks sample amplitude,
    // not a shared variable — avoids ordering races between test and worker threads.
    cfg.vad_prob_source = nullptr;

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    // Helper: feed one speech+silence cycle and wait for the ring to drain.
    // Sleeping between batches gives the worker time to process each batch so
    // the VAD state machine sees them in order (speech then silence).
    auto run_utterance = [&]() {
        for (int i = 0; i < 4; ++i) {
            auto s = MakeSpeech(4800);
            captioner.FeedAudio(s.data(), s.size());
        }
        for (int i = 0; i < 4; ++i) {
            auto s = MakeSilence(4800);
            captioner.FeedAudio(s.data(), s.size());
        }
        // Allow the worker to drain and process the batches
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    };

    size_t initial_finals = 0;
    run_utterance();
    WaitFor(
        [this, &initial_finals]() {
            auto evs = CollectEvents();
            size_t cnt = 0;
            for (const auto& ev : evs) {
                if (ev.type == CaptionEventType::Final) {
                    ++cnt;
                }
            }
            if (cnt >= 1) {
                initial_finals = cnt;
                return true;
            }
            return false;
        },
        4000);

    run_utterance();
    WaitFor(
        [this, initial_finals]() {
            auto evs = CollectEvents();
            size_t cnt = 0;
            for (const auto& ev : evs) {
                if (ev.type == CaptionEventType::Final) {
                    ++cnt;
                }
            }
            return cnt >= initial_finals + 1;
        },
        4000);

    captioner.Stop();

    auto evs = CollectEvents();
    std::vector<uint32_t> final_ids;
    for (const auto& ev : evs) {
        if (ev.type == CaptionEventType::Final) {
            final_ids.push_back(ev.utterance_id);
        }
    }

    ASSERT_GE(final_ids.size(), 2u) << "Expected at least two Final events";

    // IDs must be strictly increasing (each new Final gets utterance_id_++)
    for (size_t i = 1; i < final_ids.size(); ++i) {
        EXPECT_GT(final_ids[i], final_ids[i - 1])
            << "utterance_id must increment by 1 per Final event";
    }
}

TEST_F(LiveCaptionerTest, UtteranceId_ConstantAcrossPartials) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    cfg.partial_interval_ms = 50;         // very short so partials arrive quickly
    cfg.min_samples_for_partial = 8000;   // ~167 ms
    cfg.vad.min_silence_frames = 100000;  // no silence trigger during test

    // Constant speech
    cfg.vad_prob_source = []() { return 0.9f; };

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    // Feed enough samples to exceed min_samples_for_partial multiple times
    for (int i = 0; i < 20; ++i) {
        auto s = MakeSpeech(4800);
        captioner.FeedAudio(s.data(), s.size());
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Wait for at least 2 Partial events
    WaitFor(
        [this]() {
            auto evs = CollectEvents();
            size_t cnt = 0;
            for (const auto& ev : evs) {
                if (ev.type == CaptionEventType::Partial) {
                    ++cnt;
                }
            }
            return cnt >= 2;
        },
        4000);

    captioner.Stop();

    auto evs = CollectEvents();
    std::vector<uint32_t> partial_ids;
    for (const auto& ev : evs) {
        if (ev.type == CaptionEventType::Partial) {
            partial_ids.push_back(ev.utterance_id);
        }
    }

    if (partial_ids.size() >= 2) {
        // All partials within one utterance must share the same utterance_id
        uint32_t first_id = partial_ids.front();
        for (uint32_t id : partial_ids) {
            EXPECT_EQ(first_id, id)
                << "All Partial events for one utterance must share the same utterance_id";
        }
    }
}

// =============================================================================
// Partial event tests
// =============================================================================

TEST_F(LiveCaptionerTest, Partial_FiresDuringUtterance) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    cfg.partial_interval_ms = 50;        // short interval for quick test
    cfg.min_samples_for_partial = 4800;  // one worker batch

    // Constant high VAD, very long silence frames so no Final fires mid-test
    cfg.vad_prob_source = []() { return 0.9f; };
    cfg.vad.min_silence_frames = 100000;

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    // Feed enough samples to cross min_samples_for_partial several times over
    for (int i = 0; i < 16; ++i) {
        auto s = MakeSpeech(4800);
        captioner.FeedAudio(s.data(), s.size());
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    bool got_partial = WaitFor(
        [this]() {
            auto evs = CollectEvents();
            for (const auto& ev : evs) {
                if (ev.type == CaptionEventType::Partial) {
                    return true;
                }
            }
            return false;
        },
        4000);

    captioner.Stop();

    EXPECT_TRUE(got_partial) << "Expected at least one Partial event during utterance";
}

TEST_F(LiveCaptionerTest, Partial_ConfidenceIsZero) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    cfg.partial_interval_ms = 50;
    cfg.min_samples_for_partial = 4800;
    cfg.vad_prob_source = []() { return 0.9f; };
    cfg.vad.min_silence_frames = 100000;

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    for (int i = 0; i < 16; ++i) {
        auto s = MakeSpeech(4800);
        captioner.FeedAudio(s.data(), s.size());
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    WaitFor(
        [this]() {
            auto evs = CollectEvents();
            for (const auto& ev : evs) {
                if (ev.type == CaptionEventType::Partial) {
                    return true;
                }
            }
            return false;
        },
        4000);

    captioner.Stop();

    auto evs = CollectEvents();
    for (const auto& ev : evs) {
        if (ev.type == CaptionEventType::Partial) {
            EXPECT_FLOAT_EQ(0.0f, ev.confidence) << "Partial events must report confidence 0.0";
        }
    }
}

TEST_F(LiveCaptionerTest, Partial_SuppressedBelowMinSamples) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    cfg.partial_interval_ms = 50;
    // Require more samples than we'll ever feed before stopping
    cfg.min_samples_for_partial = 480000;  // 10 s worth — huge
    cfg.vad_prob_source = []() { return 0.9f; };
    cfg.vad.min_silence_frames = 100000;

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    // Feed only a small amount (far below min_samples_for_partial)
    for (int i = 0; i < 4; ++i) {
        auto s = MakeSpeech(4800);
        captioner.FeedAudio(s.data(), s.size());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    captioner.Stop();

    auto evs = CollectEvents();
    for (const auto& ev : evs) {
        EXPECT_NE(CaptionEventType::Partial, ev.type)
            << "No Partial events should fire when accumulation_buffer is below "
               "min_samples_for_partial";
    }
}

// =============================================================================
// Edge cases
// =============================================================================

TEST_F(LiveCaptionerTest, EdgeCase_NoCallbackSet_DoesNotCrash) {
    LiveCaptionerConfig cfg = MakeTestConfig();
    cfg.vad_prob_source = []() { return 0.9f; };
    LiveCaptioner captioner(cfg);
    // Deliberately do NOT call SetCallback()
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    auto s = MakeSpeech(4800);
    captioner.FeedAudio(s.data(), s.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    captioner.Stop();

    SUCCEED() << "No callback set — must not crash";
}

TEST_F(LiveCaptionerTest, EdgeCase_TranscribeFnReturnsEmpty_FinalEmittedWithEmptyText) {
    LiveCaptionerConfig cfg =
        MakeTestConfig([](const int16_t*, size_t, std::vector<TranscriptionSegment>& out) -> bool {
            out.clear();  // Return empty segments
            return true;
        });

    std::atomic<float> vad_prob{0.0f};
    cfg.vad_prob_source = [&]() { return vad_prob.load(); };

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    vad_prob.store(0.9f);
    for (int i = 0; i < 4; ++i) {
        auto s = MakeSpeech(4800);
        captioner.FeedAudio(s.data(), s.size());
    }
    vad_prob.store(0.0f);
    for (int i = 0; i < 4; ++i) {
        auto s = MakeSilence(4800);
        captioner.FeedAudio(s.data(), s.size());
    }

    WaitFor(
        [this]() {
            auto evs = CollectEvents();
            for (const auto& ev : evs) {
                if (ev.type == CaptionEventType::Final) {
                    return true;
                }
            }
            return false;
        },
        4000);

    captioner.Stop();

    auto evs = CollectEvents();
    for (const auto& ev : evs) {
        if (ev.type == CaptionEventType::Final) {
            EXPECT_TRUE(ev.text.empty())
                << "Empty segment list should produce empty text in Final event";
        }
    }
}

TEST_F(LiveCaptionerTest, EdgeCase_TranscribeFnReturnsFalse_FinalEmittedWithEmptyText) {
    LiveCaptionerConfig cfg =
        MakeTestConfig([](const int16_t*, size_t, std::vector<TranscriptionSegment>& out) -> bool {
            out.clear();
            return false;  // Simulate transcription failure
        });

    std::atomic<float> vad_prob{0.0f};
    cfg.vad_prob_source = [&]() { return vad_prob.load(); };

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    vad_prob.store(0.9f);
    for (int i = 0; i < 4; ++i) {
        auto s = MakeSpeech(4800);
        captioner.FeedAudio(s.data(), s.size());
    }
    vad_prob.store(0.0f);
    for (int i = 0; i < 4; ++i) {
        auto s = MakeSilence(4800);
        captioner.FeedAudio(s.data(), s.size());
    }

    WaitFor(
        [this]() {
            auto evs = CollectEvents();
            for (const auto& ev : evs) {
                if (ev.type == CaptionEventType::Final) {
                    return true;
                }
            }
            return false;
        },
        4000);

    captioner.Stop();

    SUCCEED() << "transcribe_fn returning false must not crash";
}

TEST_F(LiveCaptionerTest, EdgeCase_RmsBasedVadProb_SilenceProducesLowProb) {
    // Use default vad_prob_source (nullptr → built-in RMS estimator)
    LiveCaptionerConfig cfg = MakeTestConfig();
    cfg.vad_prob_source = nullptr;  // use built-in RMS estimator

    LiveCaptioner captioner(cfg);
    captioner.SetCallback(MakeCallback());
    ASSERT_TRUE(captioner.Initialize());
    ASSERT_TRUE(captioner.Start());

    // Feed silence — RMS estimator should produce near-zero VAD prob
    for (int i = 0; i < 8; ++i) {
        auto s = MakeSilence(4800);
        captioner.FeedAudio(s.data(), s.size());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    captioner.Stop();

    // With zero-amplitude silence, no speech should be detected → no events
    auto evs = CollectEvents();
    for (const auto& ev : evs) {
        EXPECT_NE(CaptionEventType::Final, ev.type) << "Silence should not produce a Final event";
    }
}

#endif  // ENABLE_WHISPER
