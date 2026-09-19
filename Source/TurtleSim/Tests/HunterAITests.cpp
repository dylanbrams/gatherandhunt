#include "TestFramework.h"
#include "HunterAI.h"

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

static BoardState BlankBoard(int Sz = 11) {
    BoardState B; B.Size = IntVec3(Sz, Sz, Sz); return B;
}

TEST_CASE(HunterAI, NoTurtlesNoMove) {
    BoardState B = BlankBoard();
    Obstacle H; H.Id = 1; H.Kind = EObstacleKind::Hunter; H.Position = IntVec3(5, 5, 5);
    B.Obstacles.push_back(H);
    auto Out = ComputeHunterMoves(B);
    REQUIRE(Out.size() == 1);
    REQUIRE(!Out[0].bMoves);
    REQUIRE(Out[0].IntendedCell == IntVec3(5, 5, 5));
}

TEST_CASE(HunterAI, StepsTowardClosestTurtle) {
    BoardState B = BlankBoard();
    Obstacle H; H.Id = 1; H.Kind = EObstacleKind::Hunter; H.Position = IntVec3(5, 5, 5);
    B.Obstacles.push_back(H);
    Turtle T; T.Id = 1; T.Owner = 0; T.Position = IntVec3(8, 5, 5);
    T.Facing = Orientation(EHeading::West, EPitch::Level); T.bAlive = true;
    B.Turtles.push_back(T);
    auto Out = ComputeHunterMoves(B);
    REQUIRE(Out[0].bMoves);
    REQUIRE(Out[0].IntendedCell == IntVec3(6, 5, 5));
}

TEST_CASE(HunterAI, TieBreakLowestTurtleId) {
    BoardState B = BlankBoard();
    Obstacle H; H.Id = 1; H.Kind = EObstacleKind::Hunter; H.Position = IntVec3(5, 5, 5);
    B.Obstacles.push_back(H);
    Turtle T1; T1.Id = 1; T1.Owner = 0; T1.Position = IntVec3(5, 6, 5);
    T1.Facing = Orientation(EHeading::North, EPitch::Level); T1.bAlive = true;
    Turtle T2; T2.Id = 2; T2.Owner = 0; T2.Position = IntVec3(5, 4, 5);
    T2.Facing = Orientation(EHeading::South, EPitch::Level); T2.bAlive = true;
    B.Turtles.push_back(T1);
    B.Turtles.push_back(T2);
    auto Out = ComputeHunterMoves(B);
    REQUIRE(Out[0].bMoves);
    REQUIRE(Out[0].IntendedCell == IntVec3(5, 6, 5)); // T1 (id=1) wins tie.
}

TEST_CASE(HunterAI, BlockedByStaticObstacles) {
    BoardState B = BlankBoard();
    Obstacle H; H.Id = 1; H.Kind = EObstacleKind::Hunter; H.Position = IntVec3(5, 5, 5);
    B.Obstacles.push_back(H);
    // Wall directly East; turtle further East. Hunter must go around.
    Obstacle W; W.Id = 2; W.Kind = EObstacleKind::Static; W.Position = IntVec3(6, 5, 5);
    B.Obstacles.push_back(W);
    Turtle T; T.Id = 1; T.Owner = 0; T.Position = IntVec3(7, 5, 5);
    T.Facing = Orientation(EHeading::West, EPitch::Level); T.bAlive = true;
    B.Turtles.push_back(T);
    auto Out = ComputeHunterMoves(B);
    REQUIRE(Out[0].bMoves);
    REQUIRE(Out[0].IntendedCell != IntVec3(6, 5, 5)); // didn't go through wall.
}

TEST_CASE(HunterAI, NoPathHoldsPosition) {
    BoardState B = BlankBoard(3);
    Obstacle H; H.Id = 1; H.Kind = EObstacleKind::Hunter; H.Position = IntVec3(0, 0, 0);
    B.Obstacles.push_back(H);
    // Box hunter in with statics on all 6 neighbors that exist.
    int NextId = 100;
    for (auto N : {IntVec3(1,0,0), IntVec3(0,1,0), IntVec3(0,0,1)}) {
        Obstacle S; S.Id = NextId++; S.Kind = EObstacleKind::Static; S.Position = N;
        B.Obstacles.push_back(S);
    }
    Turtle T; T.Id = 1; T.Owner = 0; T.Position = IntVec3(2, 2, 2);
    T.Facing = Orientation(EHeading::North, EPitch::Level); T.bAlive = true;
    B.Turtles.push_back(T);
    auto Out = ComputeHunterMoves(B);
    REQUIRE(!Out[0].bMoves);
}

TEST_CASE(HunterAI, IgnoresDeadAndDeliveredTurtles) {
    BoardState B = BlankBoard();
    Obstacle H; H.Id = 1; H.Kind = EObstacleKind::Hunter; H.Position = IntVec3(5, 5, 5);
    B.Obstacles.push_back(H);
    Turtle T1; T1.Id = 1; T1.Owner = 0; T1.Position = IntVec3(6, 5, 5);
    T1.Facing = Orientation(EHeading::West, EPitch::Level); T1.bAlive = false; // dead, ignore.
    Turtle T2; T2.Id = 2; T2.Owner = 0; T2.Position = IntVec3(5, 5, 8);
    T2.Facing = Orientation(EHeading::North, EPitch::Level); T2.bAlive = true; T2.bDelivered = true; // ignore.
    Turtle T3; T3.Id = 3; T3.Owner = 0; T3.Position = IntVec3(5, 8, 5);
    T3.Facing = Orientation(EHeading::North, EPitch::Level); T3.bAlive = true;
    B.Turtles.push_back(T1);
    B.Turtles.push_back(T2);
    B.Turtles.push_back(T3);
    auto Out = ComputeHunterMoves(B);
REQUIRE(Out[0].bMoves);
REQUIRE(Out[0].IntendedCell == IntVec3(5, 6, 5)); // toward T3.
}

#endif
