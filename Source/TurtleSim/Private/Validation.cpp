#include "Validation.h"
#include "CostModel.h"
#include "Orientation.h"

#include <unordered_set>

namespace turtlesim {

static bool IsKnownMoveKind(EMoveKind K) {
    switch (K) {
        case EMoveKind::Forward:
        case EMoveKind::YawLeft:
        case EMoveKind::YawRight:
        case EMoveKind::PitchUp:
        case EMoveKind::PitchDown:
        case EMoveKind::Wait:
            return true;
    }
    return false;
}

PlanValidation ValidateTurnPlan(const BoardState& State,
                                const TurnPlan& Plan,
                                std::int32_t Budget) {
    PlanValidation V;
    std::unordered_set<TurtleId> Seen;

    for (const auto& TP : Plan.PerTurtle) {
        // Duplicate per-turtle entry.
        if (!Seen.insert(TP.Turtle).second) {
            V.Error = EPlanValidationError::DuplicateTurtleEntry;
            V.OffendingTurtle = TP.Turtle;
            return V;
        }

        const Turtle* T = State.FindTurtle(TP.Turtle);
        if (!T) {
            V.Error = EPlanValidationError::TurtleUnknown;
            V.OffendingTurtle = TP.Turtle;
            return V;
        }
        if (T->Owner != Plan.Player) {
            V.Error = EPlanValidationError::TurtleNotOwned;
            V.OffendingTurtle = TP.Turtle;
            return V;
        }
        if (!T->bAlive || T->bDelivered) {
            V.Error = EPlanValidationError::TurtleNotAlive;
            V.OffendingTurtle = TP.Turtle;
            return V;
        }

        // Per-move checks: well-formedness and pitch saturation.
        // We must walk the pitch state forward across the queue because the
        // legality of a PitchUp/PitchDown depends on prior pitches in the same plan.
        EPitch CurrentPitch = T->Facing.Pitch;
        for (std::size_t i = 0; i < TP.Moves.size(); ++i) {
            const Move& M = TP.Moves[i];

            if (!IsKnownMoveKind(M.Kind)) {
                V.Error = EPlanValidationError::InvalidMoveKind;
                V.OffendingTurtle = TP.Turtle;
                V.OffendingMoveIndex = static_cast<std::int32_t>(i);
                return V;
            }

            if (M.Kind == EMoveKind::PitchUp) {
                auto Tr = TryPitchUp(CurrentPitch);
                if (!Tr.bLegal) {
                    V.Error = EPlanValidationError::InvalidPitchTransition;
                    V.OffendingTurtle = TP.Turtle;
                    V.OffendingMoveIndex = static_cast<std::int32_t>(i);
                    return V;
                }
                CurrentPitch = Tr.New;
            } else if (M.Kind == EMoveKind::PitchDown) {
                auto Tr = TryPitchDown(CurrentPitch);
                if (!Tr.bLegal) {
                    V.Error = EPlanValidationError::InvalidPitchTransition;
                    V.OffendingTurtle = TP.Turtle;
                    V.OffendingMoveIndex = static_cast<std::int32_t>(i);
                    return V;
                }
                CurrentPitch = Tr.New;
            }
            // Other kinds don't affect pitch.
        }
    }

    // Budget check last: a malformed plan should report well-formedness errors first.
    if (CostModel::TurnCost(Plan) > Budget) {
        V.Error = EPlanValidationError::BudgetExceeded;
        return V;
    }

    return V;
}

} // namespace turtlesim
