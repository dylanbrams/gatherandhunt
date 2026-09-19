#include "TestFramework.h"
#include "CostModel.h"

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

TEST_CASE(CostModel, MoveCost_Tier1) {
    for (int n = 1; n <= 6; ++n) REQUIRE(CostModel::MoveCost(n) == 1);
}

TEST_CASE(CostModel, MoveCost_Tier2) {
    for (int n = 7; n <= 10; ++n) REQUIRE(CostModel::MoveCost(n) == 2);
}

TEST_CASE(CostModel, MoveCost_Tier3) {
    for (int n = 11; n <= 30; ++n) REQUIRE(CostModel::MoveCost(n) == 3);
}

TEST_CASE(CostModel, MoveCost_NonPositiveIsZero) {
    REQUIRE(CostModel::MoveCost(0) == 0);
    REQUIRE(CostModel::MoveCost(-5) == 0);
}

TEST_CASE(CostModel, PlanCost_EmptyIsZero) {
    TurtlePlan P;
    REQUIRE(CostModel::PlanCost(P) == 0);
}

TEST_CASE(CostModel, PlanCost_SoloSprintExhaustsAtMove12) {
    // Cumulative through move 12 == 20 (the default budget).
    // 6*1 + 4*2 + 2*3 = 6 + 8 + 6 = 20.
    TurtlePlan P;
    for (int i = 0; i < 12; ++i) P.Moves.push_back(Move{EMoveKind::Forward});
    REQUIRE(CostModel::PlanCost(P) == 20);
}

TEST_CASE(CostModel, PlanCost_Through6Is6) {
    TurtlePlan P;
    for (int i = 0; i < 6; ++i) P.Moves.push_back(Move{EMoveKind::Forward});
    REQUIRE(CostModel::PlanCost(P) == 6);
}

TEST_CASE(CostModel, PlanCost_Through10Is14) {
    // 6*1 + 4*2 = 14.
    TurtlePlan P;
    for (int i = 0; i < 10; ++i) P.Moves.push_back(Move{EMoveKind::Wait});
    REQUIRE(CostModel::PlanCost(P) == 14);
}

TEST_CASE(CostModel, PlanCost_KindIrrelevant) {
    // Cost depends on queue position, not move kind.
    TurtlePlan A;
    TurtlePlan B;
    for (int i = 0; i < 8; ++i) {
        A.Moves.push_back(Move{EMoveKind::Forward});
        B.Moves.push_back(Move{EMoveKind::Wait});
    }
    REQUIRE(CostModel::PlanCost(A) == CostModel::PlanCost(B));
}

TEST_CASE(CostModel, TurnCost_SumsAcrossTurtles) {
    TurnPlan T;
    TurtlePlan A; for (int i = 0; i < 6; ++i) A.Moves.push_back(Move{EMoveKind::Forward}); // 6
    TurtlePlan B; for (int i = 0; i < 3; ++i) B.Moves.push_back(Move{EMoveKind::YawLeft}); // 3
    T.PerTurtle.push_back(A);
    T.PerTurtle.push_back(B);
    REQUIRE(CostModel::TurnCost(T) == 9);
}

TEST_CASE(CostModel, TurnCost_EmptyIsZero) {
    TurnPlan T;
    REQUIRE(CostModel::TurnCost(T) == 0);
}

#endif
