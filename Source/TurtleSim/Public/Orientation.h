#pragma once

#include <cstdint>
#include "IntVec3.h"
#include "TurtleSimAPI.h"

namespace turtlesim {

// Heading is the horizontal facing. North = +Y, East = +X, South = -Y, West = -X.
enum class EHeading : std::uint8_t {
    North = 0,
    East  = 1,
    South = 2,
    West  = 3,
};

// Pitch is the vertical tilt. Down = -Z forward, Level = horizontal, Up = +Z forward.
enum class EPitch : std::uint8_t {
    Down  = 0,
    Level = 1,
    Up    = 2,
};

struct TURTLESIM_API Orientation {
    EHeading Heading = EHeading::North;
    EPitch   Pitch   = EPitch::Level;

    constexpr Orientation() = default;
    constexpr Orientation(EHeading H, EPitch P) : Heading(H), Pitch(P) {}

    // Returns one of 6 unit vectors based on (Heading, Pitch).
    IntVec3 ForwardVector() const;

    constexpr bool operator==(const Orientation& O) const {
        return Heading == O.Heading && Pitch == O.Pitch;
    }
    constexpr bool operator!=(const Orientation& O) const { return !(*this == O); }
};

// Yaw rotates Heading by ±1 mod 4. Always valid.
TURTLESIM_API EHeading YawLeft(EHeading H);
TURTLESIM_API EHeading YawRight(EHeading H);

// Pitch transitions clamp at the ends — caller should validate first.
// Returns the new pitch and whether the transition was legal.
struct PitchTransition {
    EPitch New;
    bool   bLegal;
};

TURTLESIM_API PitchTransition TryPitchUp(EPitch P);
TURTLESIM_API PitchTransition TryPitchDown(EPitch P);

TURTLESIM_API EHeading MirrorHeadingX(EHeading H); // for board generator symmetry along X axis.

} // namespace turtlesim
