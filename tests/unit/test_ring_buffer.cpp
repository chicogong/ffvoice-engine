/**
 * @file test_ring_buffer.cpp
 * @brief Unit tests for RingBuffer class (lock-free SPSC queue)
 * @details RED Phase - These tests are designed to FAIL initially
 *          as the production code hasn't been implemented yet
 */

#include <gtest/gtest.h>
#include "utils/ring_buffer.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <random>

namespace ffvoice {
namespace test {

/**
 * Test Suite: RingBufferTest
 * Tests lock-free single-producer single-consumer ring buffer
 */
template<typename T>
class RingBufferTest : public ::testing::Test {
protected:
    static constexpr size_t DEFAULT_SIZE = 1024;
    std::unique_ptr<RingBuffer<T>> buffer_;

    void SetUp() override {
        buffer_ = std::make_unique<RingBuffer<T>>(DEFAULT_SIZE);
    }
};

// Test with different data types
using TestTypes = ::testing::Types<float, int, double, int16_t>;
TYPED_TEST_SUITE(RingBufferTest, TestTypes);

// ============================================================================
// Basic Operations Tests
// ============================================================================

TYPED_TEST(RingBufferTest, CreateBufferWithValidSize) {
    // UT-BUF-001: Create buffer with valid size
    RingBuffer<TypeParam> buffer(256);

    EXPECT_EQ(buffer.capacity(), 256)
        << "Buffer capacity should match requested size";

    EXPECT_EQ(buffer.size(), 0)
        << "New buffer should be empty";

    EXPECT_TRUE(buffer.empty())
        << "New buffer should report as empty";

    EXPECT_FALSE(buffer.full())
        << "New buffer should not be full";
}

TYPED_TEST(RingBufferTest, WriteAndReadData) {
    // UT-BUF-002: Write and read data
    const TypeParam test_value = static_cast<TypeParam>(42);

    EXPECT_TRUE(this->buffer_->push(test_value))
        << "Should successfully push to empty buffer";

    EXPECT_EQ(this->buffer_->size(), 1)
        << "Size should be 1 after push";

    EXPECT_FALSE(this->buffer_->empty())
        << "Buffer should not be empty after push";

    TypeParam read_value;
    EXPECT_TRUE(this->buffer_->pop(read_value))
        << "Should successfully pop from buffer";

    EXPECT_EQ(read_value, test_value)
        << "Read value should match written value";

    EXPECT_TRUE(this->buffer_->empty())
        << "Buffer should be empty after pop";
}

TYPED_TEST(RingBufferTest, HandleFullBuffer) {
    // UT-BUF-003: Handle full buffer
    // Fill the buffer
    for (size_t i = 0; i < this->buffer_->capacity(); ++i) {
        TypeParam value = static_cast<TypeParam>(i);
        EXPECT_TRUE(this->buffer_->push(value))
            << "Should push value " << i;
    }

    EXPECT_TRUE(this->buffer_->full())
        << "Buffer should be full";

    EXPECT_EQ(this->buffer_->size(), this->buffer_->capacity())
        << "Size should equal capacity when full";

    // Try to push one more - should fail
    TypeParam extra_value = static_cast<TypeParam>(999);
    EXPECT_FALSE(this->buffer_->push(extra_value))
        << "Should fail to push to full buffer";

    // Read one and push again
    TypeParam read_value;
    EXPECT_TRUE(this->buffer_->pop(read_value));
    EXPECT_TRUE(this->buffer_->push(extra_value))
        << "Should push after making space";
}

TYPED_TEST(RingBufferTest, HandleEmptyBuffer) {
    // UT-BUF-004: Handle empty buffer
    EXPECT_TRUE(this->buffer_->empty())
        << "New buffer should be empty";

    TypeParam value;
    EXPECT_FALSE(this->buffer_->pop(value))
        << "Should fail to pop from empty buffer";

    // Value should be unchanged
    TypeParam original = static_cast<TypeParam>(123);
    value = original;
    EXPECT_FALSE(this->buffer_->pop(value));
    EXPECT_EQ(value, original)
        << "Failed pop should not modify output parameter";
}

TYPED_TEST(RingBufferTest, TestCapacityAndAvailableSpace) {
    // UT-BUF-005: Test capacity and available space
    size_t capacity = this->buffer_->capacity();

    EXPECT_EQ(this->buffer_->available_write(), capacity)
        << "Empty buffer should have full capacity available for writing";

    EXPECT_EQ(this->buffer_->available_read(), 0)
        << "Empty buffer should have nothing available for reading";

    // Push half capacity
    size_t half = capacity / 2;
    for (size_t i = 0; i < half; ++i) {
        this->buffer_->push(static_cast<TypeParam>(i));
    }

    EXPECT_EQ(this->buffer_->available_write(), capacity - half)
        << "Should have half capacity available for writing";

    EXPECT_EQ(this->buffer_->available_read(), half)
        << "Should have half capacity available for reading";

    // Read quarter capacity
    size_t quarter = capacity / 4;
    for (size_t i = 0; i < quarter; ++i) {
        TypeParam value;
        this->buffer_->pop(value);
    }

    EXPECT_EQ(this->buffer_->available_write(), capacity - half + quarter)
        << "Available write space should increase after reading";

    EXPECT_EQ(this->buffer_->available_read(), half - quarter)
        << "Available read should decrease after reading";
}

// ============================================================================
// Concurrency Tests
// ============================================================================

TYPED_TEST(RingBufferTest, ConcurrentSingleProducerSingleConsumer) {
    // UT-BUF-006: Concurrent single producer, single consumer
    const size_t num_items = 100000;
    std::atomic<bool> all_written{false};
    std::vector<TypeParam> written_values;
    std::vector<TypeParam> read_values;

    // Reserve space to avoid reallocation
    written_values.reserve(num_items);
    read_values.reserve(num_items);

    // Producer thread
    std::thread producer([&]() {
        for (size_t i = 0; i < num_items; ++i) {
            TypeParam value = static_cast<TypeParam>(i);
            while (!this->buffer_->push(value)) {
                // Busy wait - in production would use condition variable
                std::this_thread::yield();
            }
            written_values.push_back(value);
        }
        all_written.store(true);
    });

    // Consumer thread
    std::thread consumer([&]() {
        size_t items_read = 0;
        while (items_read < num_items) {
            TypeParam value;
            if (this->buffer_->pop(value)) {
                read_values.push_back(value);
                items_read++;
            } else if (all_written.load() && this->buffer_->empty()) {
                break; // No more data coming
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify all data was transferred correctly
    EXPECT_EQ(read_values.size(), num_items)
        << "Should read all items";

    EXPECT_EQ(written_values.size(), num_items)
        << "Should write all items";

    // Verify data integrity
    for (size_t i = 0; i < num_items; ++i) {
        EXPECT_EQ(read_values[i], written_values[i])
            << "Data mismatch at index " << i;
    }

    EXPECT_TRUE(this->buffer_->empty())
        << "Buffer should be empty after consuming all data";
}

TYPED_TEST(RingBufferTest, NoDataCorruptionUnderLoad) {
    // UT-BUF-007: No data corruption under load
    const size_t num_items = 50000;
    std::atomic<bool> stop_producer{false};

    // Use a pattern to detect corruption
    auto make_pattern = [](size_t i) -> TypeParam {
        // Create a pattern that's easy to verify
        return static_cast<TypeParam>((i * 31337) ^ 0xDEADBEEF);
    };

    std::atomic<size_t> corruption_count{0};

    // Producer thread - writes pattern
    std::thread producer([&]() {
        size_t i = 0;
        while (i < num_items) {
            TypeParam value = make_pattern(i);
            if (this->buffer_->push(value)) {
                i++;
            }
            // No yield - maximum pressure
        }
        stop_producer.store(true);
    });

    // Consumer thread - verifies pattern
    std::thread consumer([&]() {
        size_t expected_index = 0;
        while (expected_index < num_items) {
            TypeParam value;
            if (this->buffer_->pop(value)) {
                TypeParam expected = make_pattern(expected_index);
                if (value != expected) {
                    corruption_count.fetch_add(1);
                }
                expected_index++;
            } else if (stop_producer.load() && this->buffer_->empty()) {
                break;
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(corruption_count.load(), 0)
        << "Should have no data corruption";
}

TYPED_TEST(RingBufferTest, LockFreePerformanceVerification) {
    // UT-BUF-008: Lock-free performance verification
    const size_t num_operations = 1000000;

    // Measure single-threaded baseline
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_operations; ++i) {
        TypeParam value = static_cast<TypeParam>(i);
        if (this->buffer_->push(value)) {
            TypeParam read_value;
            this->buffer_->pop(read_value);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto single_threaded_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Reset buffer
    this->buffer_->clear();

    // Measure concurrent performance
    start = std::chrono::high_resolution_clock::now();

    std::thread producer([&]() {
        for (size_t i = 0; i < num_operations; ++i) {
            TypeParam value = static_cast<TypeParam>(i);
            while (!this->buffer_->push(value)) {
                // Busy wait
            }
        }
    });

    std::thread consumer([&]() {
        for (size_t i = 0; i < num_operations; ++i) {
            TypeParam value;
            while (!this->buffer_->pop(value)) {
                // Busy wait
            }
        }
    });

    producer.join();
    consumer.join();

    end = std::chrono::high_resolution_clock::now();
    auto concurrent_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Concurrent should be faster or at least comparable
    double speedup = static_cast<double>(single_threaded_duration.count()) /
                    static_cast<double>(concurrent_duration.count());

    EXPECT_GT(speedup, 0.5)
        << "Concurrent performance should be at least half of single-threaded"
        << " (actual speedup: " << speedup << "x)";

    // Check operations per second
    double ops_per_second = (num_operations * 1000000.0) / concurrent_duration.count();
    EXPECT_GT(ops_per_second, 1000000)
        << "Should achieve at least 1M ops/second";
}

TYPED_TEST(RingBufferTest, StressTestWithRapidWritesReads) {
    // UT-BUF-009: Stress test with rapid writes/reads
    const size_t test_duration_ms = 1000; // 1 second stress test
    std::atomic<bool> stop_test{false};
    std::atomic<size_t> total_writes{0};
    std::atomic<size_t> total_reads{0};
    std::atomic<size_t> write_failures{0};
    std::atomic<size_t> read_failures{0};

    // Random data generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 1000);

    // Writer thread - writes as fast as possible
    std::thread writer([&]() {
        while (!stop_test.load()) {
            TypeParam value = static_cast<TypeParam>(dis(gen));
            if (this->buffer_->push(value)) {
                total_writes.fetch_add(1);
            } else {
                write_failures.fetch_add(1);
            }
        }
    });

    // Reader thread - reads as fast as possible
    std::thread reader([&]() {
        while (!stop_test.load()) {
            TypeParam value;
            if (this->buffer_->pop(value)) {
                total_reads.fetch_add(1);
            } else {
                read_failures.fetch_add(1);
            }
        }
    });

    // Let it run
    std::this_thread::sleep_for(std::chrono::milliseconds(test_duration_ms));
    stop_test.store(true);

    writer.join();
    reader.join();

    // Drain remaining items
    TypeParam value;
    while (this->buffer_->pop(value)) {
        total_reads.fetch_add(1);
    }

    EXPECT_EQ(total_writes.load(), total_reads.load())
        << "Total writes should equal total reads";

    size_t total_operations = total_writes.load() + total_reads.load();
    double ops_per_second = (total_operations * 1000.0) / test_duration_ms;

    EXPECT_GT(ops_per_second, 100000)
        << "Should handle at least 100K operations per second under stress";

    // Failure rate should be reasonable (buffer full/empty conditions)
    double write_failure_rate = static_cast<double>(write_failures.load()) /
                               (total_writes.load() + write_failures.load());
    double read_failure_rate = static_cast<double>(read_failures.load()) /
                              (total_reads.load() + read_failures.load());

    EXPECT_LT(write_failure_rate, 0.5)
        << "Write failure rate should be reasonable";
    EXPECT_LT(read_failure_rate, 0.5)
        << "Read failure rate should be reasonable";
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST(RingBufferEdgeCaseTest, BufferSizeOne) {
    // UT-BUF-010: Buffer size = 1 element
    RingBuffer<int> tiny_buffer(1);

    EXPECT_EQ(tiny_buffer.capacity(), 1);
    EXPECT_TRUE(tiny_buffer.empty());

    // Push one element
    EXPECT_TRUE(tiny_buffer.push(42));
    EXPECT_TRUE(tiny_buffer.full());
    EXPECT_EQ(tiny_buffer.size(), 1);

    // Can't push another
    EXPECT_FALSE(tiny_buffer.push(43));

    // Pop the element
    int value;
    EXPECT_TRUE(tiny_buffer.pop(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(tiny_buffer.empty());

    // Can push again
    EXPECT_TRUE(tiny_buffer.push(44));
}

TEST(RingBufferEdgeCaseTest, BufferSizeMax) {
    // UT-BUF-011: Buffer size = max size
    const size_t max_size = 1024 * 1024; // 1MB elements
    RingBuffer<uint8_t> large_buffer(max_size);

    EXPECT_EQ(large_buffer.capacity(), max_size);
    EXPECT_TRUE(large_buffer.empty());

    // Fill it up
    for (size_t i = 0; i < max_size; ++i) {
        EXPECT_TRUE(large_buffer.push(static_cast<uint8_t>(i & 0xFF)));
    }

    EXPECT_TRUE(large_buffer.full());

    // Read it all back
    for (size_t i = 0; i < max_size; ++i) {
        uint8_t value;
        EXPECT_TRUE(large_buffer.pop(value));
        EXPECT_EQ(value, static_cast<uint8_t>(i & 0xFF));
    }

    EXPECT_TRUE(large_buffer.empty());
}

TEST(RingBufferEdgeCaseTest, WriteExactCapacity) {
    // UT-BUF-012: Write exact capacity
    const size_t capacity = 100;
    RingBuffer<int> buffer(capacity);

    // Write exactly capacity items
    for (size_t i = 0; i < capacity; ++i) {
        EXPECT_TRUE(buffer.push(static_cast<int>(i)));
    }

    EXPECT_TRUE(buffer.full());
    EXPECT_EQ(buffer.size(), capacity);

    // Verify we can read them all back in order
    for (size_t i = 0; i < capacity; ++i) {
        int value;
        EXPECT_TRUE(buffer.pop(value));
        EXPECT_EQ(value, static_cast<int>(i));
    }

    EXPECT_TRUE(buffer.empty());
}

TEST(RingBufferEdgeCaseTest, WrapAroundBehavior) {
    // UT-BUF-013: Wrap-around behavior
    const size_t capacity = 10;
    RingBuffer<int> buffer(capacity);

    // Fill buffer
    for (int i = 0; i < capacity; ++i) {
        buffer.push(i);
    }

    // Read half
    for (int i = 0; i < capacity / 2; ++i) {
        int value;
        buffer.pop(value);
    }

    // Write more (will wrap around)
    for (int i = capacity; i < capacity + capacity / 2; ++i) {
        EXPECT_TRUE(buffer.push(i));
    }

    // Should have correct values in order
    std::vector<int> expected;
    for (int i = capacity / 2; i < capacity + capacity / 2; ++i) {
        expected.push_back(i);
    }

    std::vector<int> actual;
    int value;
    while (buffer.pop(value)) {
        actual.push_back(value);
    }

    EXPECT_EQ(actual, expected)
        << "Wrap-around should preserve order";
}

// ============================================================================
// Batch Operations Tests (if supported)
// ============================================================================

TEST(RingBufferBatchTest, BatchPushAndPop) {
    // Additional test for batch operations if implemented
    RingBuffer<float> buffer(1024);

    // Prepare batch data
    std::vector<float> batch_in(100);
    for (size_t i = 0; i < batch_in.size(); ++i) {
        batch_in[i] = static_cast<float>(i) * 0.1f;
    }

    // Batch push
    EXPECT_EQ(buffer.push_bulk(batch_in.data(), batch_in.size()), batch_in.size())
        << "Should push all items in batch";

    EXPECT_EQ(buffer.size(), batch_in.size());

    // Batch pop
    std::vector<float> batch_out(100);
    EXPECT_EQ(buffer.pop_bulk(batch_out.data(), batch_out.size()), batch_in.size())
        << "Should pop all items in batch";

    // Verify data
    EXPECT_EQ(batch_in, batch_out)
        << "Batch data should match";
}

TEST(RingBufferBatchTest, PartialBatchOperations) {
    // Test partial batch operations when buffer is partially full
    RingBuffer<int> buffer(50);

    // Fill partially
    for (int i = 0; i < 30; ++i) {
        buffer.push(i);
    }

    // Try to push more than available space
    std::vector<int> batch_in(30);
    std::iota(batch_in.begin(), batch_in.end(), 100);

    size_t pushed = buffer.push_bulk(batch_in.data(), batch_in.size());
    EXPECT_EQ(pushed, 20) // Only 20 spaces left
        << "Should push only available space";

    EXPECT_TRUE(buffer.full());

    // Pop partial
    std::vector<int> batch_out(100);
    size_t popped = buffer.pop_bulk(batch_out.data(), batch_out.size());
    EXPECT_EQ(popped, 50) // Only 50 items in buffer
        << "Should pop only available items";

    EXPECT_TRUE(buffer.empty());
}

} // namespace test
} // namespace ffvoice