#pragma once
#include <cstdint>

namespace mdt
{
/** xorshift32. Every chain owns one, seeded from the engine's, so a render is repeatable. */
struct Rng
{
    uint32_t s = 0x9e3779b9u;

    Rng() noexcept = default;
    explicit Rng (uint32_t seed) noexcept : s (seed ? seed : 0x9e3779b9u) {}

    uint32_t next() noexcept
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }

    /** [0, 1), like `Math.random()`. */
    float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
};
} // namespace mdt
