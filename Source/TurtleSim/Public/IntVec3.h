#pragma once

#include <cstddef>
#include <cstdint>

namespace turtlesim {

struct IntVec3 {
    std::int32_t X = 0;
    std::int32_t Y = 0;
    std::int32_t Z = 0;

    constexpr IntVec3() = default;
    constexpr IntVec3(std::int32_t InX, std::int32_t InY, std::int32_t InZ)
        : X(InX), Y(InY), Z(InZ) {}

    constexpr IntVec3 operator+(const IntVec3& O) const { return {X + O.X, Y + O.Y, Z + O.Z}; }
    constexpr IntVec3 operator-(const IntVec3& O) const { return {X - O.X, Y - O.Y, Z - O.Z}; }
    constexpr bool operator==(const IntVec3& O) const { return X == O.X && Y == O.Y && Z == O.Z; }
    constexpr bool operator!=(const IntVec3& O) const { return !(*this == O); }

    // Lex-order for deterministic sorting / map keys.
    constexpr bool operator<(const IntVec3& O) const {
        if (X != O.X) return X < O.X;
        if (Y != O.Y) return Y < O.Y;
        return Z < O.Z;
    }
};

struct IntVec3Hash {
    std::size_t operator()(const IntVec3& V) const noexcept {
        // Splittable-mix style; fine for hash tables, not used in determinism-sensitive paths.
        std::uint64_t h = static_cast<std::uint32_t>(V.X);
        h = h * 0x9E3779B97F4A7C15ull + static_cast<std::uint32_t>(V.Y);
        h = h * 0x9E3779B97F4A7C15ull + static_cast<std::uint32_t>(V.Z);
        return static_cast<std::size_t>(h ^ (h >> 32));
    }
};

} // namespace turtlesim
