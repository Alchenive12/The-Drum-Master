// File: dsp/include/plugin_dsp/SpscQueue.h
// Lock-free single-producer / single-consumer ring (audio thread -> UI thread), fixed capacity.
// push() never blocks or allocates: when full, the item is dropped (the UI just misses a shape).
#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace plugin_dsp
{
template <typename T, std::size_t Capacity>
class SpscQueue
{
    static_assert (Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    bool push (const T& item) noexcept
    {
        const std::size_t w = writePos.load (std::memory_order_relaxed);
        if (w - readPos.load (std::memory_order_acquire) >= Capacity) return false;
        items[w & (Capacity - 1)] = item;
        writePos.store (w + 1, std::memory_order_release);
        return true;
    }

    bool pop (T& item) noexcept
    {
        const std::size_t r = readPos.load (std::memory_order_relaxed);
        if (r == writePos.load (std::memory_order_acquire)) return false;
        item = items[r & (Capacity - 1)];
        readPos.store (r + 1, std::memory_order_release);
        return true;
    }

private:
    std::array<T, Capacity> items {};
    std::atomic<std::size_t> writePos { 0 }, readPos { 0 };
};
} // namespace plugin_dsp
