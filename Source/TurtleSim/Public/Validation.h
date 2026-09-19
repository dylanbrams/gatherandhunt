#pragma once

#include <cstdint>
#include "BoardState.h"
#include "Move.h"
#include "TurtleSimAPI.h"

namespace turtlesim {

enum class EPlanValidationError : std::uint8_t {
    None = 0,
    BudgetExceeded,
    TurtleNotOwned,
    TurtleNotAlive,
    TurtleUnknown,
    DuplicateTurtleEntry,
    InvalidPitchTransition,  // PitchUp at Up, PitchDown at Down.
    InvalidMoveKind,         // out-of-range EMoveKind value.
};

struct PlanValidation {
    EPlanValidationError Error = EPlanValidationError::None;
    std::int32_t OffendingTurtle    = -1;
    std::int32_t OffendingMoveIndex = -1;
};

// Validates a single player's turn plan against the current board state.
// Side-effect free; runs identically client/server.
//
// Budget is the total number of cost units allowed across all of the
// player's turtles for one turn; pass DefaultMoveBudget by default.
TURTLESIM_API PlanValidation ValidateTurnPlan(const BoardState& State,
                                              const TurnPlan& Plan,
                                              std::int32_t Budget);

} // namespace turtlesim
