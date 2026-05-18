"""
Tests for the ffvoice.RingBuffer class (lock-free SPSC ring buffer).
"""

import pytest


def test_construction_and_capacity():
    """A new ring buffer reports the requested capacity and starts empty."""
    try:
        from ffvoice import RingBuffer

        rb = RingBuffer(8)
        assert rb.capacity() == 8
        assert rb.size() == 0
        assert rb.empty() is True
        assert rb.full() is False
        assert rb.available_read() == 0
        assert rb.available_write() == 8
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_empty_state():
    """empty() is True only while no elements are stored."""
    try:
        from ffvoice import RingBuffer

        rb = RingBuffer(4)
        assert rb.empty() is True

        assert rb.push(42) is True
        assert rb.empty() is False

        assert rb.pop() == 42
        assert rb.empty() is True
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_full_state():
    """full() becomes True exactly when capacity elements are stored."""
    try:
        from ffvoice import RingBuffer

        rb = RingBuffer(3)
        assert rb.full() is False

        for value in (1, 2, 3):
            assert rb.push(value) is True

        assert rb.full() is True
        assert rb.size() == 3
        assert rb.available_write() == 0
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_push_then_pop_returns_same_value():
    """A single pushed value is returned unchanged by pop()."""
    try:
        from ffvoice import RingBuffer

        rb = RingBuffer(4)
        assert rb.push(1234) is True
        assert rb.pop() == 1234
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_fifo_order():
    """Values are popped in the same order they were pushed (FIFO)."""
    try:
        from ffvoice import RingBuffer

        rb = RingBuffer(8)
        values = [10, 20, 30, 40, 50]
        for value in values:
            assert rb.push(value) is True

        popped = [rb.pop() for _ in range(len(values))]
        assert popped == values
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_pop_on_empty_returns_none():
    """pop() returns None when the buffer is empty."""
    try:
        from ffvoice import RingBuffer

        rb = RingBuffer(4)
        assert rb.pop() is None

        # Still None after a push/pop round-trip drains the buffer again.
        assert rb.push(7) is True
        assert rb.pop() == 7
        assert rb.pop() is None
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_push_on_full_returns_false():
    """push() returns False once the buffer is full and stores nothing extra."""
    try:
        from ffvoice import RingBuffer

        rb = RingBuffer(3)
        for value in (1, 2, 3):
            assert rb.push(value) is True

        # Buffer is full: the next push must fail.
        assert rb.push(4) is False
        assert rb.size() == 3

        # The rejected value was not stored; FIFO content is unchanged.
        assert rb.pop() == 1
        assert rb.pop() == 2
        assert rb.pop() == 3
        assert rb.empty() is True
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_size_and_availability_tracking():
    """size / available_read / available_write stay consistent during use."""
    try:
        from ffvoice import RingBuffer

        capacity = 5
        rb = RingBuffer(capacity)

        for i in range(capacity):
            assert rb.push(i) is True
            stored = i + 1
            assert rb.size() == stored
            assert rb.available_read() == stored
            assert rb.available_write() == capacity - stored

        # available_read always equals size; the two free counters sum to capacity.
        for i in range(capacity):
            assert rb.pop() == i
            remaining = capacity - i - 1
            assert rb.size() == remaining
            assert rb.available_read() == remaining
            assert rb.available_write() == capacity - remaining
            assert rb.available_read() + rb.available_write() == capacity
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_clear_empties_buffer():
    """clear() resets the buffer to the empty state."""
    try:
        from ffvoice import RingBuffer

        rb = RingBuffer(4)
        for value in (1, 2, 3):
            assert rb.push(value) is True
        assert rb.size() == 3

        rb.clear()
        assert rb.size() == 0
        assert rb.empty() is True
        assert rb.full() is False
        assert rb.available_read() == 0
        assert rb.available_write() == 4
        assert rb.pop() is None

        # The buffer is fully usable again after clear().
        assert rb.push(99) is True
        assert rb.pop() == 99
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_wrap_around():
    """Pushing past the end of the underlying storage preserves FIFO order."""
    try:
        from ffvoice import RingBuffer

        capacity = 4
        rb = RingBuffer(capacity)

        # Fill the buffer completely.
        for value in (1, 2, 3, 4):
            assert rb.push(value) is True
        assert rb.full() is True

        # Pop part of it, freeing slots at the start of the storage.
        assert rb.pop() == 1
        assert rb.pop() == 2
        assert rb.size() == 2

        # Push more: these elements wrap around to the freed slots.
        assert rb.push(5) is True
        assert rb.push(6) is True
        assert rb.full() is True

        # Everything still comes out in FIFO order across the wrap point.
        assert [rb.pop() for _ in range(4)] == [3, 4, 5, 6]
        assert rb.empty() is True
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_push_bulk_returns_count_pushed():
    """push_bulk() copies the whole array when there is room and returns its length."""
    try:
        import numpy as np

        from ffvoice import RingBuffer

        rb = RingBuffer(16)
        data = np.array([1, 2, 3, 4, 5], dtype=np.int16)

        pushed = rb.push_bulk(data)
        assert pushed == 5
        assert rb.size() == 5

        # Values land in FIFO order.
        assert [rb.pop() for _ in range(5)] == [1, 2, 3, 4, 5]
    except ImportError as e:
        pytest.skip(f"Module not built or numpy missing: {e}")


def test_push_bulk_partial_when_near_full():
    """push_bulk() only pushes what fits and returns the partial count."""
    try:
        import numpy as np

        from ffvoice import RingBuffer

        rb = RingBuffer(5)

        # Pre-fill so only 2 slots remain free.
        assert rb.push(100) is True
        assert rb.push(101) is True
        assert rb.push(102) is True
        assert rb.available_write() == 2

        data = np.array([1, 2, 3, 4], dtype=np.int16)
        pushed = rb.push_bulk(data)
        assert pushed == 2
        assert rb.full() is True

        # The first 3 elements plus the 2 that fit, all in FIFO order.
        assert [rb.pop() for _ in range(5)] == [100, 101, 102, 1, 2]

        # Pushing into a full buffer pushes nothing.
        assert rb.push(1) is True  # one slot now free again is irrelevant; refill
        rb.clear()
        for value in (1, 2, 3, 4, 5):
            assert rb.push(value) is True
        assert rb.push_bulk(np.array([9, 9], dtype=np.int16)) == 0
    except ImportError as e:
        pytest.skip(f"Module not built or numpy missing: {e}")


def test_pop_bulk_returns_values_and_count():
    """pop_bulk() returns the requested number of values as an int16 ndarray."""
    try:
        import numpy as np

        from ffvoice import RingBuffer

        rb = RingBuffer(16)
        for value in (10, 20, 30, 40, 50):
            assert rb.push(value) is True

        out = rb.pop_bulk(3)
        assert isinstance(out, np.ndarray)
        assert out.dtype == np.int16
        assert out.ndim == 1
        assert len(out) == 3
        assert list(out) == [10, 20, 30]

        # Remaining elements stay in FIFO order.
        assert rb.size() == 2
        assert [rb.pop() for _ in range(2)] == [40, 50]
    except ImportError as e:
        pytest.skip(f"Module not built or numpy missing: {e}")


def test_pop_bulk_fewer_than_requested_when_near_empty():
    """pop_bulk() returns only what is available when asked for too many."""
    try:
        import numpy as np

        from ffvoice import RingBuffer

        rb = RingBuffer(16)
        for value in (1, 2):
            assert rb.push(value) is True

        # Ask for more than is stored: get exactly what is available.
        out = rb.pop_bulk(10)
        assert isinstance(out, np.ndarray)
        assert out.dtype == np.int16
        assert len(out) == 2
        assert list(out) == [1, 2]
        assert rb.empty() is True

        # pop_bulk on an empty buffer returns an empty array.
        empty = rb.pop_bulk(5)
        assert isinstance(empty, np.ndarray)
        assert len(empty) == 0
    except ImportError as e:
        pytest.skip(f"Module not built or numpy missing: {e}")


def test_bulk_round_trip_with_wrap_around():
    """push_bulk / pop_bulk keep FIFO order across a wrap-around."""
    try:
        import numpy as np

        from ffvoice import RingBuffer

        capacity = 6
        rb = RingBuffer(capacity)

        # Fill, drain part, then bulk-push so the data wraps the storage.
        assert rb.push_bulk(np.array([1, 2, 3, 4, 5, 6], dtype=np.int16)) == 6
        first = rb.pop_bulk(4)
        assert list(first) == [1, 2, 3, 4]

        assert rb.push_bulk(np.array([7, 8, 9, 10], dtype=np.int16)) == 4
        assert rb.full() is True

        rest = rb.pop_bulk(capacity)
        assert list(rest) == [5, 6, 7, 8, 9, 10]
        assert rb.empty() is True
    except ImportError as e:
        pytest.skip(f"Module not built or numpy missing: {e}")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
