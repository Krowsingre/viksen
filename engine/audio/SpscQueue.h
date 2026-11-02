#pragma once
// Bounded, lock-free single-producer / single-consumer queue.
// Used to pass commands to the audio thread and return retired objects from it.
// T must be trivially copyable (pointers, small PODs).

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace atmos {

template <typename T, size_t Capacity>
class SpscQueue {
    static_assert(std::is_trivially_copyable_v<T>, "SpscQueue requires trivially copyable T");
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    bool push(const T& value) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_acquire);
        if (head - tail >= Capacity) return false; // full
        slots_[head & (Capacity - 1)] = value;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& out) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);
        if (tail == head) return false; // empty
        out = slots_[tail & (Capacity - 1)];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

private:
    std::array<T, Capacity> slots_{};
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};

} // namespace atmos
