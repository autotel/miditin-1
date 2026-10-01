#pragma once
#include <array>
#include <atomic>

/** One writer, one reader, latest value wins, neither side ever blocks or allocates.

    The writer fills its private back slot and swaps it with the shared middle slot; the reader
    swaps the middle into its private front slot only when something new arrived. Three slots, so
    the writer never has to wait for the reader to finish. Used for `Patch` (message -> audio) and
    `UiSnapshot` (audio -> message). */
namespace mdt
{
template <typename T>
class TripleBuffer
{
public:
    /** Writer. Fill `back()`, then `publish()`. */
    T& back() noexcept { return slots[(size_t) backIndex]; }

    void publish() noexcept
    {
        backIndex = middle.exchange (backIndex | freshBit, std::memory_order_acq_rel) & indexMask;
    }

    /** Reader. Returns true when `front()` changed. */
    bool fetch() noexcept
    {
        if ((middle.load (std::memory_order_relaxed) & freshBit) == 0)
            return false;
        frontIndex = middle.exchange (frontIndex, std::memory_order_acq_rel) & indexMask;
        return true;
    }

    const T& front() const noexcept { return slots[(size_t) frontIndex]; }

private:
    static constexpr int freshBit = 4, indexMask = 3;
    std::array<T, 3> slots {};
    int backIndex = 0, frontIndex = 1;
    std::atomic<int> middle { 2 };
};
} // namespace mdt
