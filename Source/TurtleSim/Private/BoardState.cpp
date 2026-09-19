#include "BoardState.h"

namespace turtlesim {

bool BoardState::IsInBounds(const IntVec3& P) const {
    return P.X >= 0 && P.X < Size.X
        && P.Y >= 0 && P.Y < Size.Y
        && P.Z >= 0 && P.Z < Size.Z;
}

const Turtle* BoardState::FindTurtle(TurtleId Id) const {
    for (const auto& T : Turtles) if (T.Id == Id) return &T;
    return nullptr;
}
Turtle* BoardState::FindTurtleMut(TurtleId Id) {
    for (auto& T : Turtles) if (T.Id == Id) return &T;
    return nullptr;
}

const Obstacle* BoardState::FindObstacle(ObstacleId Id) const {
    for (const auto& O : Obstacles) if (O.Id == Id) return &O;
    return nullptr;
}
Obstacle* BoardState::FindObstacleMut(ObstacleId Id) {
    for (auto& O : Obstacles) if (O.Id == Id) return &O;
    return nullptr;
}

const Objective* BoardState::FindObjective(ObjectiveId Id) const {
    for (const auto& O : Objectives) if (O.Id == Id) return &O;
    return nullptr;
}
Objective* BoardState::FindObjectiveMut(ObjectiveId Id) {
    for (auto& O : Objectives) if (O.Id == Id) return &O;
    return nullptr;
}

} // namespace turtlesim
