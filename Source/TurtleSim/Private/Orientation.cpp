#include "Orientation.h"

namespace turtlesim {

IntVec3 Orientation::ForwardVector() const {
    if (Pitch == EPitch::Up)   return {0, 0, +1};
    if (Pitch == EPitch::Down) return {0, 0, -1};
    switch (Heading) {
        case EHeading::North: return {0, +1, 0};
        case EHeading::East:  return {+1, 0, 0};
        case EHeading::South: return {0, -1, 0};
        case EHeading::West:  return {-1, 0, 0};
    }
    return {0, 0, 0};
}

EHeading YawLeft(EHeading H) {
    return static_cast<EHeading>((static_cast<std::uint8_t>(H) + 3) & 0x3);
}

EHeading YawRight(EHeading H) {
    return static_cast<EHeading>((static_cast<std::uint8_t>(H) + 1) & 0x3);
}

PitchTransition TryPitchUp(EPitch P) {
    switch (P) {
        case EPitch::Down:  return {EPitch::Level, true};
        case EPitch::Level: return {EPitch::Up,    true};
        case EPitch::Up:    return {EPitch::Up,    false};
    }
    return {P, false};
}

PitchTransition TryPitchDown(EPitch P) {
    switch (P) {
        case EPitch::Up:    return {EPitch::Level, true};
        case EPitch::Level: return {EPitch::Down,  true};
        case EPitch::Down:  return {EPitch::Down,  false};
    }
    return {P, false};
}

EHeading MirrorHeadingX(EHeading H) {
    switch (H) {
        case EHeading::East: return EHeading::West;
        case EHeading::West: return EHeading::East;
        default:             return H;
    }
}

} // namespace turtlesim
