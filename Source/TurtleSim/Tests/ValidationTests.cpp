#include "TestFramework.h"
#include "Validation.h"
#include "Constants.h"

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

static BoardState MakeBoardWithTurtle(PlayerId Owner, EPitch StartPitch = EPitch::Level) {
    BoardState B;
    B.Size = IntVec3(11, 11, 11);
    Turtle T;
    T.Id = 1;
    T.Owner = Owner;
    T.Position = IntVec3(0, 5, 5);
    T.Facing = Orientation(EHeading::East, StartPitch);
    T.bAlive = true;
    B.Turtles.push_back(T);
    return B;
}

static TurtlePlan PlanFor(TurtleId Id, std::initializer_list<EMoveKind> Kinds) {
    TurtlePlan P; P.Turtle = Id;
    for (auto K : Kinds) P.Moves.push_back(Move{K});
    return P;
}

TEST_CASE(Validation, ValidEmptyPlan) {
    BoardState B = MakeBoardWithTurtle(0);
    TurnPlan T; T.Player = 0;
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::None);
}

TEST_CASE(Validation, ValidSimplePlan) {
    BoardState B = MakeBoardWithTurtle(0);
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::Forward, EMoveKind::YawLeft, EMoveKind::Wait}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::None);
}

TEST_CASE(Validation, BudgetExceeded) {
    BoardState B = MakeBoardWithTurtle(0);
    TurnPlan T; T.Player = 0;
    TurtlePlan P; P.Turtle = 1;
    for (int i = 0; i < 13; ++i) P.Moves.push_back(Move{EMoveKind::Forward}); // cost = 23 > 20.
    T.PerTurtle.push_back(P);
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::BudgetExceeded);
}

TEST_CASE(Validation, BudgetExactlyAtLimitIsValid) {
    BoardState B = MakeBoardWithTurtle(0);
    TurnPlan T; T.Player = 0;
    TurtlePlan P; P.Turtle = 1;
    for (int i = 0; i < 12; ++i) P.Moves.push_back(Move{EMoveKind::Forward}); // cost = 20.
    T.PerTurtle.push_back(P);
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::None);
}

TEST_CASE(Validation, TurtleNotOwned) {
    BoardState B = MakeBoardWithTurtle(/*Owner*/1);
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::Forward}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::TurtleNotOwned);
    REQUIRE(V.OffendingTurtle == 1);
}

TEST_CASE(Validation, TurtleUnknown) {
    BoardState B = MakeBoardWithTurtle(0);
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(99, {EMoveKind::Forward}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::TurtleUnknown);
}

TEST_CASE(Validation, TurtleNotAlive) {
    BoardState B = MakeBoardWithTurtle(0);
    B.Turtles[0].bAlive = false;
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::Forward}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::TurtleNotAlive);
}

TEST_CASE(Validation, TurtleDeliveredCountsAsNotAlive) {
    BoardState B = MakeBoardWithTurtle(0);
    B.Turtles[0].bDelivered = true;
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::Forward}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::TurtleNotAlive);
}

TEST_CASE(Validation, DuplicateTurtleEntry) {
    BoardState B = MakeBoardWithTurtle(0);
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::Forward}));
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::YawLeft}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::DuplicateTurtleEntry);
}

TEST_CASE(Validation, PitchUpFromUpIsInvalid) {
    BoardState B = MakeBoardWithTurtle(0, EPitch::Up);
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::PitchUp}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::InvalidPitchTransition);
    REQUIRE(V.OffendingMoveIndex == 0);
}

TEST_CASE(Validation, PitchDownFromDownIsInvalid) {
    BoardState B = MakeBoardWithTurtle(0, EPitch::Down);
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::PitchDown}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::InvalidPitchTransition);
}

TEST_CASE(Validation, PitchSequenceTracksAcrossQueue) {
    // From Level: PitchUp (Level→Up), PitchUp (Up→ILLEGAL).
    BoardState B = MakeBoardWithTurtle(0, EPitch::Level);
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::PitchUp, EMoveKind::PitchUp}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::InvalidPitchTransition);
    REQUIRE(V.OffendingMoveIndex == 1);
}

TEST_CASE(Validation, PitchUpThenPitchDownIsLegal) {
    BoardState B = MakeBoardWithTurtle(0, EPitch::Level);
    TurnPlan T; T.Player = 0;
    T.PerTurtle.push_back(PlanFor(1, {EMoveKind::PitchUp, EMoveKind::PitchDown}));
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::None);
}

TEST_CASE(Validation, InvalidMoveKindCaught) {
    BoardState B = MakeBoardWithTurtle(0);
    TurnPlan T; T.Player = 0;
    TurtlePlan P; P.Turtle = 1;
    Move M;
    M.Kind = static_cast<EMoveKind>(99);
    P.Moves.push_back(M);
    T.PerTurtle.push_back(P);
    auto V = ValidateTurnPlan(B, T, DefaultMoveBudget);
    REQUIRE(V.Error == EPlanValidationError::InvalidMoveKind);
}

#endif
