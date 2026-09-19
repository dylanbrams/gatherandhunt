#include "TestFramework.h"
#include "BoardState.h"

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

static BoardState MakeBoard11() {
    BoardState B;
    B.Size = IntVec3(11, 11, 11);
    return B;
}

TEST_CASE(BoardState, IsInBounds_BasicCorners) {
    BoardState B = MakeBoard11();
    REQUIRE(B.IsInBounds(IntVec3(0, 0, 0)));
    REQUIRE(B.IsInBounds(IntVec3(10, 10, 10)));
    REQUIRE(!B.IsInBounds(IntVec3(-1, 0, 0)));
    REQUIRE(!B.IsInBounds(IntVec3(11, 0, 0)));
    REQUIRE(!B.IsInBounds(IntVec3(0, -1, 0)));
    REQUIRE(!B.IsInBounds(IntVec3(0, 11, 0)));
    REQUIRE(!B.IsInBounds(IntVec3(0, 0, -1)));
    REQUIRE(!B.IsInBounds(IntVec3(0, 0, 11)));
}

TEST_CASE(BoardState, FindTurtleByIdReturnsCorrectEntry) {
    BoardState B = MakeBoard11();

    Turtle T1; T1.Id = 1; T1.Owner = 0; T1.Position = IntVec3(1, 2, 3);
    Turtle T2; T2.Id = 2; T2.Owner = 1; T2.Position = IntVec3(4, 5, 6);
    B.Turtles.push_back(T1);
    B.Turtles.push_back(T2);

    const Turtle* Found = B.FindTurtle(2);
    REQUIRE(Found != nullptr);
    REQUIRE(Found->Owner == 1);
    REQUIRE(Found->Position == IntVec3(4, 5, 6));
    REQUIRE(B.FindTurtle(99) == nullptr);
}

TEST_CASE(BoardState, FindObjectiveByIdReturnsCorrectEntry) {
    BoardState B = MakeBoard11();
    Objective O; O.Id = 10; O.Position = IntVec3(0, 0, 0); O.PointValue = 5;
    B.Objectives.push_back(O);
    const Objective* F = B.FindObjective(10);
    REQUIRE(F != nullptr);
    REQUIRE(F->PointValue == 5);
}

TEST_CASE(IntVec3, ArithmeticAndOrdering) {
    IntVec3 a(1, 2, 3);
    IntVec3 b(4, 5, 6);
    REQUIRE(a + b == IntVec3(5, 7, 9));
    REQUIRE(b - a == IntVec3(3, 3, 3));
    REQUIRE(a < b);
    REQUIRE(!(b < a));

IntVec3 c(1, 2, 4);
REQUIRE(a < c); // tiebreak by Z
}

#endif
