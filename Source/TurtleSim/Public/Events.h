#pragma once

#include <cstdint>
#include <vector>
#include "IntVec3.h"
#include "Orientation.h"

namespace turtlesim {

enum class EEventType : std::uint8_t {
    TurtleTurned,
    TurtleWaited,
    TurtleBlocked,           // attempted forward into wall.
    TurtleMoved,
    TurtleDestroyed,
    TurtleDelivered,
    TurtleClaimedObjective,
    ObstacleMoved,
    ObstacleDestroyed,
    HunterRetargeted,
};

struct TickEvent {
    EEventType   Type            = EEventType::TurtleWaited;
    std::int32_t PrimaryId       = -1; // turtle or obstacle id.
    std::int32_t SecondaryId     = -1; // -1 if N/A; else target/destroyer/objective id.
    IntVec3      FromPosition;
    IntVec3      ToPosition;
    Orientation  FromOrientation;
    Orientation  ToOrientation;
    std::int32_t PointsDelta     = 0;
};

struct TickResult {
    std::int32_t           TickIndex = 0;
    std::vector<TickEvent> Events;
};

} // namespace turtlesim
