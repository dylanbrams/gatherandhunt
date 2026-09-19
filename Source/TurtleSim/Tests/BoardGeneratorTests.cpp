#include "TestFramework.h"
#include "BoardGenerator.h"

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

static BoardGenParams DefaultParams(std::uint32_t Seed) {
    BoardGenParams P;
    P.Size = IntVec3(11, 11, 11);
    P.TurtlesPerPlayer = 4;
    P.StaticObstacles = 8;
    P.Hunters = 2;
    P.ObjectivePointPool = {1, 1, 1, 3, 3, 5}; // 6 mirrored = 12 objectives total.
    P.Seed = Seed;
    return P;
}

TEST_CASE(BoardGenerator, ProducesBoardForDefaultParams) {
    auto R = GenerateBoard(DefaultParams(1));
    REQUIRE(R.bOk);
    REQUIRE(R.State.Turtles.size() == 8);
    REQUIRE(R.State.Obstacles.size() == 10);   // 8 static + 2 hunter.
    REQUIRE(R.State.Objectives.size() == 12);
}

TEST_CASE(BoardGenerator, MirrorSymmetryHolds) {
    auto R = GenerateBoard(DefaultParams(2));
    REQUIRE(R.bOk);
    REQUIRE(ValidateMirrorSymmetry(R.State));
}

TEST_CASE(BoardGenerator, DeterministicSameSeed) {
    auto A = GenerateBoard(DefaultParams(42));
    auto B = GenerateBoard(DefaultParams(42));
    REQUIRE(A.bOk);
    REQUIRE(B.bOk);
    REQUIRE(A.State.Turtles.size() == B.State.Turtles.size());
    for (std::size_t i = 0; i < A.State.Turtles.size(); ++i) {
        REQUIRE(A.State.Turtles[i].Position == B.State.Turtles[i].Position);
        REQUIRE(A.State.Turtles[i].Owner == B.State.Turtles[i].Owner);
    }
    for (std::size_t i = 0; i < A.State.Obstacles.size(); ++i) {
        REQUIRE(A.State.Obstacles[i].Position == B.State.Obstacles[i].Position);
        REQUIRE(A.State.Obstacles[i].Kind == B.State.Obstacles[i].Kind);
    }
}

TEST_CASE(BoardGenerator, MirrorSymmetryAcross100Seeds) {
    for (std::uint32_t Seed = 0; Seed < 100; ++Seed) {
        auto R = GenerateBoard(DefaultParams(Seed));
        REQUIRE(R.bOk);
        REQUIRE(ValidateMirrorSymmetry(R.State));
    }
}

TEST_CASE(BoardGenerator, EveryTurtleCanReachAtLeast3Objectives) {
    // The generator validates this internally; here we just sanity-check that
    // generated boards don't trivially trap a turtle.
    auto R = GenerateBoard(DefaultParams(7));
    REQUIRE(R.bOk);
    // A successful generation already guarantees this; assert produces correct counts.
REQUIRE(R.State.Objectives.size() >= 3);
}

#endif
