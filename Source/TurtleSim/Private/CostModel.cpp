#include "CostModel.h"

namespace turtlesim::CostModel {

std::int32_t PlanCost(const TurtlePlan& Plan) {
    std::int32_t Total = 0;
    for (std::size_t i = 0; i < Plan.Moves.size(); ++i) {
        Total += MoveCost(static_cast<std::int32_t>(i) + 1);
    }
    return Total;
}

std::int32_t TurnCost(const TurnPlan& Plan) {
    std::int32_t Total = 0;
    for (const auto& TP : Plan.PerTurtle) {
        Total += PlanCost(TP);
    }
    return Total;
}

} // namespace turtlesim::CostModel
