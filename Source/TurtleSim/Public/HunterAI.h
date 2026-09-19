#pragma once

#include <cstdint>
#include <vector>
#include "BoardState.h"
#include "TurtleSimAPI.h"

namespace turtlesim {

struct HunterMoveDecision {
    ObstacleId Hunter = -1;
    IntVec3    IntendedCell; // == hunter's cell when no path exists.
    bool       bMoves = false;
};

// Computes one-step intents for all living Hunters. BFS shortest-path to nearest
// living, undelivered turtle, treating other living obstacles and out-of-bounds
// as blocked. Ties broken by lowest TurtleId. Stable across runs.
TURTLESIM_API std::vector<HunterMoveDecision> ComputeHunterMoves(const BoardState& State);

} // namespace turtlesim
