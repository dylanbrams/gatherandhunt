#pragma once

#include <cstdint>
#include "Move.h"
#include "TurtleSimAPI.h"

namespace turtlesim::CostModel {

// Cost of the Nth move (1-indexed) in a single turtle's queue.
// Tier 1: moves 1-6, cost 1.  Tier 2: 7-10, cost 2.  Tier 3: 11+, cost 3.
constexpr std::int32_t MoveCost(std::int32_t OneIndexedMoveNumber) {
    if (OneIndexedMoveNumber <= 0)  return 0;
    if (OneIndexedMoveNumber <= 6)  return 1;
    if (OneIndexedMoveNumber <= 10) return 2;
    return 3;
}

TURTLESIM_API std::int32_t PlanCost(const TurtlePlan& Plan);
TURTLESIM_API std::int32_t TurnCost(const TurnPlan& Plan);

} // namespace turtlesim::CostModel
