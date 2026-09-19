#include "TestFramework.h"
#include "BoardGenerator.h"
#include "Resolver.h"
#include "RandomPCG.h"
#include "CostModel.h"
#include "Validation.h"
#include "Constants.h"

#include <algorithm>
#include <vector>

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

static TurnPlan RandomPlan(const BoardState& B, PlayerId Player, std::uint32_t Seed) {
    PCG32 R(Seed);
    TurnPlan P; P.Player = Player;

    // Collect this player's living turtles, ordered by Id for determinism.
    std::vector<TurtleId> Owned;
    for (const auto& T : B.Turtles) {
        if (T.Owner == Player && T.bAlive && !T.bDelivered) Owned.push_back(T.Id);
    }
    std::sort(Owned.begin(), Owned.end());

    // For each turtle, draft a small plan; ensure the entire turn fits in budget.
    std::int32_t RemainingBudget = DefaultMoveBudget;
    for (TurtleId Tid : Owned) {
        TurtlePlan TP; TP.Turtle = Tid;
        // Up to 6 moves per turtle, but stop if we'd exceed budget.
        const std::int32_t MaxLen = R.RangeInt32(0, 5);
        EPitch SimPitch = B.FindTurtle(Tid)->Facing.Pitch;
        for (std::int32_t i = 0; i < MaxLen; ++i) {
            const std::int32_t MoveNo = static_cast<std::int32_t>(TP.Moves.size()) + 1;
            const std::int32_t Cost = CostModel::MoveCost(MoveNo);
            if (Cost > RemainingBudget) break;

            // Pick a move kind avoiding illegal pitch transitions.
            EMoveKind K;
            for (;;) {
                K = static_cast<EMoveKind>(R.RangeInt32(0, 5));
                if (K == EMoveKind::PitchUp && SimPitch == EPitch::Up) continue;
                if (K == EMoveKind::PitchDown && SimPitch == EPitch::Down) continue;
                break;
            }
            if (K == EMoveKind::PitchUp)   SimPitch = TryPitchUp(SimPitch).New;
            if (K == EMoveKind::PitchDown) SimPitch = TryPitchDown(SimPitch).New;

            TP.Moves.push_back(Move{K});
            RemainingBudget -= Cost;
        }
        P.PerTurtle.push_back(std::move(TP));
    }
    return P;
}

static bool EventsEqual(const TickEvent& A, const TickEvent& B) {
    return A.Type == B.Type
        && A.PrimaryId == B.PrimaryId
        && A.SecondaryId == B.SecondaryId
        && A.FromPosition == B.FromPosition
        && A.ToPosition == B.ToPosition
        && A.PointsDelta == B.PointsDelta;
}

static bool TurnResultEqual(const TurnResult& A, const TurnResult& B) {
    if (A.Ticks.size() != B.Ticks.size()) return false;
    for (std::size_t i = 0; i < A.Ticks.size(); ++i) {
        if (A.Ticks[i].Events.size() != B.Ticks[i].Events.size()) return false;
        for (std::size_t j = 0; j < A.Ticks[i].Events.size(); ++j) {
            if (!EventsEqual(A.Ticks[i].Events[j], B.Ticks[i].Events[j])) return false;
        }
    }
    if (A.FinalState.ScorePlayer0 != B.FinalState.ScorePlayer0) return false;
    if (A.FinalState.ScorePlayer1 != B.FinalState.ScorePlayer1) return false;
    if (A.FinalState.Turtles.size() != B.FinalState.Turtles.size()) return false;
    for (std::size_t i = 0; i < A.FinalState.Turtles.size(); ++i) {
        if (A.FinalState.Turtles[i].Position != B.FinalState.Turtles[i].Position) return false;
        if (A.FinalState.Turtles[i].bAlive != B.FinalState.Turtles[i].bAlive) return false;
        if (A.FinalState.Turtles[i].bDelivered != B.FinalState.Turtles[i].bDelivered) return false;
    }
    return true;
}

TEST_CASE(DeterminismFuzz, ResolverIsByteIdenticalAcrossRuns) {
    BoardGenParams GP;
    GP.Size = IntVec3(11, 11, 11);
    GP.TurtlesPerPlayer = 3;
    GP.StaticObstacles = 4;
    GP.Hunters = 2;
    GP.ObjectivePointPool = {1, 1, 3, 5};

    constexpr std::int32_t Iterations = 200;  // 1000 ideal but keep test fast.
    for (std::int32_t i = 0; i < Iterations; ++i) {
        GP.Seed = static_cast<std::uint32_t>(i + 1);
        auto Board = GenerateBoard(GP);
        REQUIRE(Board.bOk);

        auto P0 = RandomPlan(Board.State, 0, GP.Seed * 2);
        auto P1 = RandomPlan(Board.State, 1, GP.Seed * 2 + 1);

        // Validation should pass (we generated within budget).
        auto V0 = ValidateTurnPlan(Board.State, P0, DefaultMoveBudget);
        auto V1 = ValidateTurnPlan(Board.State, P1, DefaultMoveBudget);
        REQUIRE(V0.Error == EPlanValidationError::None);
        REQUIRE(V1.Error == EPlanValidationError::None);

        auto R1 = ResolveTurn(Board.State, P0, P1);
        auto R2 = ResolveTurn(Board.State, P0, P1);
        REQUIRE(TurnResultEqual(R1, R2));
    }
}

TEST_CASE(DeterminismFuzz, MultiTurnSoakNoCrashes) {
    BoardGenParams GP;
    GP.Size = IntVec3(9, 9, 9);
    GP.TurtlesPerPlayer = 3;
    GP.StaticObstacles = 4;
    GP.Hunters = 2;
    GP.ObjectivePointPool = {1, 3, 5};
    GP.Seed = 12345;
    auto Board = GenerateBoard(GP);
    REQUIRE(Board.bOk);

    BoardState State = Board.State;
    for (std::int32_t Turn = 0; Turn < 20; ++Turn) {
        auto P0 = RandomPlan(State, 0, 100u + Turn * 2u);
        auto P1 = RandomPlan(State, 1, 101u + Turn * 2u);
        auto R = ResolveTurn(State, P0, P1);
        State = R.FinalState;
        // Sanity: scores never go negative.
        REQUIRE(State.ScorePlayer0 >= 0);
        REQUIRE(State.ScorePlayer1 >= 0);
    }
}

#endif
