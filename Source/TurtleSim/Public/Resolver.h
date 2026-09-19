#pragma once

#include <cstdint>
#include <vector>
#include "BoardState.h"
#include "Move.h"
#include "Events.h"
#include "TurtleSimAPI.h"

namespace turtlesim {

struct TurnResult {
    std::vector<TickResult> Ticks;
    BoardState              FinalState;
};

// Pure, total, deterministic. Same (InitialState, Plan0, Plan1) -> identical TurnResult.
// Plans whose Player field is wrong are still accepted (caller is responsible for
// validation via ValidateTurnPlan); the resolver only consumes per-turtle plans.
TURTLESIM_API TurnResult ResolveTurn(const BoardState& InitialState,
                                    const TurnPlan& Plan0,
                                    const TurnPlan& Plan1);

} // namespace turtlesim
