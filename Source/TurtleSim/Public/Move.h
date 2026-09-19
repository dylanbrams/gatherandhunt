#pragma once

#include <cstdint>
#include <vector>

namespace turtlesim {

using TurtleId    = std::int32_t;
using ObstacleId  = std::int32_t;
using ObjectiveId = std::int32_t;
using PlayerId    = std::int32_t;

enum class EMoveKind : std::uint8_t {
    Forward   = 0,
    YawLeft   = 1,
    YawRight  = 2,
    PitchUp   = 3,
    PitchDown = 4,
    Wait      = 5,
};

struct Move {
    EMoveKind Kind = EMoveKind::Wait;
};

struct TurtlePlan {
    TurtleId          Turtle = -1;
    std::vector<Move> Moves;
};

struct TurnPlan {
    PlayerId                Player = -1;
    std::vector<TurtlePlan> PerTurtle;
};

} // namespace turtlesim
