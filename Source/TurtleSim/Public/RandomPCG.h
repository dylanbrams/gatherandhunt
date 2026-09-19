#pragma once

#include <cstdint>
#include "TurtleSimAPI.h"

namespace turtlesim {

// PCG XSH-RR 64-state, 32-bit output. Algorithm by Melissa O'Neill, MIT licensed.
// See https://www.pcg-random.org/. Reimplemented in-tree to keep the sim core
// dependency-free and deterministic across platforms.
class TURTLESIM_API PCG32 {
public:
    constexpr PCG32() = default;
    explicit PCG32(std::uint64_t Seed, std::uint64_t StreamId = 0xDA3E39CB94B95BDBull);

    void Reseed(std::uint64_t Seed, std::uint64_t StreamId = 0xDA3E39CB94B95BDBull);

    std::uint32_t NextUInt32();

    // Uniform integer in [Min, Max] inclusive. Uses bounded rejection to avoid bias.
    std::int32_t  RangeInt32(std::int32_t Min, std::int32_t Max);

private:
    std::uint64_t State = 0x853C49E6748FEA9Bull;
    std::uint64_t Inc   = 0xDA3E39CB94B95BDBull; // must be odd.
};

} // namespace turtlesim
