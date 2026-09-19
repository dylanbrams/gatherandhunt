#pragma once

#include <cstdint>
#include <vector>
#include "BoardState.h"
#include "TurtleSimAPI.h"

namespace turtlesim {

struct BoardGenParams {
    IntVec3       Size              = IntVec3(11, 11, 11);
    std::int32_t  TurtlesPerPlayer  = 4;
    std::int32_t  StaticObstacles   = 8;     // total across the board (will be even after mirroring).
    std::int32_t  Hunters           = 2;     // total.
    std::vector<std::int32_t> ObjectivePointPool; // will be mirrored; total objectives = 2 * size of this pool.
    std::uint32_t Seed              = 0;
    std::int32_t  MaxRetries        = 32;
};

struct BoardGenResult {
    BoardState State;
    bool       bOk = false;     // false if generator gave up after MaxRetries.
    std::int32_t Retries = 0;
};

// Deterministic generator: same Params → same BoardState. Mirrors player-1 setup
// across the X midplane onto player 2 (spec §12.2). Validates reachability before
// returning (spec §12.1 step 8).
TURTLESIM_API BoardGenResult GenerateBoard(const BoardGenParams& Params);

// Validates that the generated board is X-mirror-symmetric.
// Returns true iff every player-0 piece has a mirrored player-1 counterpart.
TURTLESIM_API bool ValidateMirrorSymmetry(const BoardState& B);

} // namespace turtlesim
