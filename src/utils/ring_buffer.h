/**
 * @file ring_buffer.h
 * @brief Lock-free single-producer / single-consumer (SPSC) ring buffer
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>

namespace ffvoice {

/**
 * @brief Lock-free SPSC ring buffer.
 *
 * A bounded FIFO queue that is safe for exactly one producer thread
 * (push / push_bulk) and one consumer thread (pop / pop_bulk) running
 * concurrently. It takes no locks and performs no allocation after
 * construction, which makes it suitable for the real-time audio path — for
 * example handing samples from the PortAudio capture callback to a processing
 * or encoding thread.
 *
 * It is NOT safe for multiple producers or multiple consumers. clear() must
 * only be called while neither the producer nor the consumer thread is running.
 *
 * @tparam T element type (use a trivially copyable type on the real-time path)
 */
template <typename T>
class RingBuffer {
public:
    /**
     * @brief Construct a ring buffer able to hold @p capacity elements.
     * @param capacity Maximum number of elements (clamped to at least 1).
     */
    explicit RingBuffer(size_t capacity)
        : capacity_(capacity == 0 ? 1 : capacity), buffer_(capacity_) {
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /// Maximum number of elements the buffer can hold.
    size_t capacity() const noexcept {
        return capacity_;
    }

    /// Number of elements currently stored.
    size_t size() const noexcept {
        return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
    }

    /// True when no elements are stored.
    bool empty() const noexcept {
        return size() == 0;
    }

    /// True when the buffer cannot accept any more elements.
    bool full() const noexcept {
        return size() >= capacity_;
    }

    /// Number of elements that can be pushed before the buffer becomes full.
    size_t available_write() const noexcept {
        return capacity_ - size();
    }

    /// Number of elements that can be popped before the buffer becomes empty.
    size_t available_read() const noexcept {
        return size();
    }

    /**
     * @brief Push one element. Producer thread only.
     * @return false if the buffer is full (the element is not stored).
     */
    bool push(const T& value) {
        const size_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) >= capacity_) {
            return false;  // full
        }
        buffer_[head % capacity_] = value;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pop one element. Consumer thread only.
     * @param value Receives the popped element on success; left untouched on failure.
     * @return false if the buffer is empty.
     */
    bool pop(T& value) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        value = buffer_[tail % capacity_];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Push up to @p count elements. Producer thread only.
     * @return Number of elements actually pushed (0..count, limited by free space).
     */
    size_t push_bulk(const T* data, size_t count) {
        if (data == nullptr || count == 0) {
            return 0;
        }
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t free_space = capacity_ - (head - tail_.load(std::memory_order_acquire));
        const size_t n = std::min(count, free_space);
        for (size_t i = 0; i < n; ++i) {
            buffer_[(head + i) % capacity_] = data[i];
        }
        head_.store(head + n, std::memory_order_release);
        return n;
    }

    /**
     * @brief Pop up to @p count elements. Consumer thread only.
     * @return Number of elements actually popped (0..count, limited by stored data).
     */
    size_t pop_bulk(T* data, size_t count) {
        if (data == nullptr || count == 0) {
            return 0;
        }
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t available = head_.load(std::memory_order_acquire) - tail;
        const size_t n = std::min(count, available);
        for (size_t i = 0; i < n; ++i) {
            data[i] = buffer_[(tail + i) % capacity_];
        }
        tail_.store(tail + n, std::memory_order_release);
        return n;
    }

    /**
     * @brief Reset the buffer to empty.
     *
     * Only safe to call when neither the producer nor the consumer thread is
     * running — it is not synchronized against concurrent push/pop.
     */
    void clear() noexcept {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

private:
    // Cache line size used to keep the producer and consumer counters apart.
    // 128 bytes covers Apple Silicon (128-byte lines) and x86-64 (64-byte lines).
    static constexpr size_t kCacheLine = 128;

    // head_ and tail_ are monotonically increasing counters; the number of live
    // elements is (head_ - tail_). Slots are addressed by (index % capacity_),
    // so any capacity is supported, not only powers of two. The counters would
    // only wrap after 2^64 operations, which is unreachable in practice.
    //
    // head_ and tail_ are placed on separate cache lines: the producer only
    // writes head_ and the consumer only writes tail_, so sharing a line would
    // cause false sharing and ping-pong the line between the two cores.
    const size_t capacity_;
    std::vector<T> buffer_;
    alignas(kCacheLine) std::atomic<size_t> head_{0};  // elements ever pushed (producer writes)
    alignas(kCacheLine) std::atomic<size_t> tail_{0};  // elements ever popped (consumer writes)
};

}  // namespace ffvoice
