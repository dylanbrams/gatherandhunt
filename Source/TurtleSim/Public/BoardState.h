#pragma once

#include <cstdint>
#include <vector>
#include "IntVec3.h"
#include "Orientation.h"
#include "Move.h"
#include "TurtleSimAPI.h"

namespace turtlesim {

struct Turtle {
    TurtleId    Id            = -1;
    PlayerId    Owner         = -1;
    IntVec3     Position;
    Orientation Facing;
    bool        bAlive        = true;
    bool        bDelivered    = false;
    std::int32_t CarriedPoints = 0;
};

enum class EObstacleKind : std::uint8_t {
    Static    = 0,
    Wandering = 1, // deferred to v1.1; placeholder
    Hunter    = 2,
};

enum class EWanderingPattern : std::uint8_t {
    None         = 0,
    LinearPatrol = 1,
    CircularLoop = 2,
};

struct Obstacle {
    ObstacleId    Id     = -1;
    EObstacleKind Kind   = EObstacleKind::Static;
    IntVec3       Position;
    EWanderingPattern Pattern = EWanderingPattern::None;
    IntVec3       OriginPosition;
    Orientation   PatternOrientation;
    std::int32_t  StepDistance = 1;
    std::int32_t  PrimaryStepCount = 1;
    std::int32_t  SecondaryStepCount = 1;
    std::int32_t  PhaseOffset = 0;
    bool          bAlive = true;
};

struct Objective {
    ObjectiveId  Id         = -1;
    IntVec3      Position;
    std::int32_t PointValue = 1;
    bool         bClaimed   = false;
};

struct TURTLESIM_API BoardState {
    IntVec3                Size{0, 0, 0};
    std::int32_t           TurnNumber   = 0;
    std::vector<Turtle>    Turtles;
    std::vector<Obstacle>  Obstacles;
    std::vector<Objective> Objectives;
    std::int32_t           ScorePlayer0 = 0;
    std::int32_t           ScorePlayer1 = 0;
    std::uint32_t          Seed         = 0;

    bool IsInBounds(const IntVec3& P) const;

    const Turtle* FindTurtle(TurtleId Id) const;
    Turtle*       FindTurtleMut(TurtleId Id);

    const Obstacle* FindObstacle(ObstacleId Id) const;
    Obstacle*       FindObstacleMut(ObstacleId Id);

    const Objective* FindObjective(ObjectiveId Id) const;
    Objective*       FindObjectiveMut(ObjectiveId Id);
};

} // namespace turtlesim
