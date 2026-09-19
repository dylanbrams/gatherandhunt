#pragma once

#include <cstdint>
#include <vector>
#include "BoardState.h"
#include "Resolver.h"
#include "TurtleSimAPI.h"

namespace turtlesim {

// What happened to a turtle on one tick of a resolved turn. Used by planner UIs
// to draw the "ghost" path of a queued plan (spec §16.1).
enum class ETrackStepKind : std::uint8_t {
    Start     = 0, // pre-turn position; always the first step.
    Moved     = 1,
    Turned    = 2,
    Waited    = 3,
    Blocked   = 4,
    Destroyed = 5,
    Delivered = 6, // Position is the off-grid cell the turtle left toward.
};

struct TrackStep {
    std::int32_t   TickIndex = -1; // -1 for Start.
    ETrackStepKind Kind      = ETrackStepKind::Start;
    IntVec3        Position;
    Orientation    Facing;
    bool           bClaimedObjective = false;
    std::int32_t   PointsDelta       = 0; // claim or delivery points on this tick.
};

// Walks a resolved turn and returns one step per tick in which the turtle did
// something, preceded by a Start step. Empty if the turtle isn't in StartState.
// Pure and deterministic; events are consumed in log order.
TURTLESIM_API std::vector<TrackStep> ExtractTurtleTrack(const BoardState& StartState,
                                                       const TurnResult& Result,
                                                       TurtleId Turtle);

} // namespace turtlesim
