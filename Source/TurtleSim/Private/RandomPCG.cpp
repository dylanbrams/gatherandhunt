#include "RandomPCG.h"

namespace turtlesim {

PCG32::PCG32(std::uint64_t Seed, std::uint64_t StreamId) {
    Reseed(Seed, StreamId);
}

void PCG32::Reseed(std::uint64_t Seed, std::uint64_t StreamId) {
    State = 0u;
    Inc   = (StreamId << 1u) | 1u;
    NextUInt32();
    State += Seed;
    NextUInt32();
}

std::uint32_t PCG32::NextUInt32() {
    const std::uint64_t OldState = State;
    State = OldState * 6364136223846793005ull + Inc;
    const std::uint32_t Xorshifted = static_cast<std::uint32_t>(((OldState >> 18u) ^ OldState) >> 27u);
    const std::uint32_t Rot = static_cast<std::uint32_t>(OldState >> 59u);
    return (Xorshifted >> Rot) | (Xorshifted << ((-static_cast<std::int32_t>(Rot)) & 31));
}

std::int32_t PCG32::RangeInt32(std::int32_t Min, std::int32_t Max) {
    if (Max <= Min) return Min;
    const std::uint32_t Span = static_cast<std::uint32_t>(Max - Min) + 1u;
    // Bounded-rejection to eliminate bias.
    const std::uint32_t Threshold = static_cast<std::uint32_t>(-static_cast<std::int32_t>(Span)) % Span;
    for (;;) {
        const std::uint32_t R = NextUInt32();
        if (R >= Threshold) {
            return Min + static_cast<std::int32_t>(R % Span);
        }
    }
}

} // namespace turtlesim
