#include "TestFramework.h"
#include "Resolver.h"

#include <algorithm>

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

// Test helpers ---------------------------------------------------------------

static BoardState BlankBoard(int Sz = 11) {
    BoardState B;
    B.Size = IntVec3(Sz, Sz, Sz);
    return B;
}

static Turtle MakeTurtle(TurtleId Id, PlayerId Owner, IntVec3 Pos, EHeading H, EPitch P = EPitch::Level) {
    Turtle T;
    T.Id = Id; T.Owner = Owner; T.Position = Pos;
    T.Facing = Orientation(H, P);
    T.bAlive = true;
    return T;
}

static Obstacle MakeStatic(ObstacleId Id, IntVec3 Pos) {
    Obstacle O; O.Id = Id; O.Kind = EObstacleKind::Static; O.Position = Pos; return O;
}

static Obstacle MakeHunter(ObstacleId Id, IntVec3 Pos) {
    Obstacle O; O.Id = Id; O.Kind = EObstacleKind::Hunter; O.Position = Pos; return O;
}

static TurtlePlan PlanFor(TurtleId Id, std::initializer_list<EMoveKind> Kinds) {
    TurtlePlan P; P.Turtle = Id;
    for (auto K : Kinds) P.Moves.push_back(Move{K});
    return P;
}

static TurnPlan SinglePlan(PlayerId Player, TurtlePlan TP) {
    TurnPlan T; T.Player = Player;
    T.PerTurtle.push_back(std::move(TP));
    return T;
}

static const Turtle* T(const BoardState& S, TurtleId Id) { return S.FindTurtle(Id); }

// Helpers for assertions.
static bool IsAlive(const BoardState& S, TurtleId Id) {
    const Turtle* X = S.FindTurtle(Id);
    return X && X->bAlive;
}
static bool IsAliveObstacle(const BoardState& S, ObstacleId Id) {
    const Obstacle* X = S.FindObstacle(Id);
    return X && X->bAlive;
}

// Case 1: two turtles head-on into the same cell — both destroyed.
TEST_CASE(Collision, Case01_HeadOnSameCell) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(6, 5, 5), EHeading::West));

    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Forward})));

    REQUIRE(!IsAlive(R.FinalState, 1));
    REQUIRE(!IsAlive(R.FinalState, 2));
}

// Case 2: two turtles swapping cells — both destroyed.
TEST_CASE(Collision, Case02_PositionSwap) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(5, 5, 5), EHeading::West));
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Forward})));
    REQUIRE(!IsAlive(R.FinalState, 1));
    REQUIRE(!IsAlive(R.FinalState, 2));
}

// Case 3: A enters stationary B from the rear — B destroyed, A occupies the cell.
TEST_CASE(Collision, Case03_RearHitStationary) {
    BoardState B = BlankBoard();
    // A at (4,5,5) facing East; B at (5,5,5) facing East (waiting). A's forward hits B's rear.
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(5, 5, 5), EHeading::East));
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Wait})));
    REQUIRE(IsAlive(R.FinalState, 1));
    REQUIRE(!IsAlive(R.FinalState, 2));
    REQUIRE(T(R.FinalState, 1)->Position == IntVec3(5, 5, 5));
}

// Case 4: A enters stationary B from the side — B destroyed, A occupies.
TEST_CASE(Collision, Case04_SideHitStationary) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));   // moves into (5,5,5)
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(5, 5, 5), EHeading::North));  // facing perpendicular
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Wait})));
    REQUIRE(IsAlive(R.FinalState, 1));
    REQUIRE(!IsAlive(R.FinalState, 2));
    REQUIRE(T(R.FinalState, 1)->Position == IntVec3(5, 5, 5));
}

// Case 5: A enters B from front (B stationary, facing A) — both destroyed.
TEST_CASE(Collision, Case05_FrontalHitOnStationaryFacingTowardMover) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));   // moves East into (5,5,5)
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(5, 5, 5), EHeading::West));   // stationary, facing West (toward A)
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Wait})));
    REQUIRE(!IsAlive(R.FinalState, 1));
    REQUIRE(!IsAlive(R.FinalState, 2));
}

// Case 6: three turtles enter the same cell — all three destroyed.
TEST_CASE(Collision, Case06_ThreeWayPileup) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(6, 5, 5), EHeading::West));
    B.Turtles.push_back(MakeTurtle(3, 0, IntVec3(5, 4, 5), EHeading::North));
    TurnPlan P0; P0.Player = 0;
    P0.PerTurtle.push_back(PlanFor(1, {EMoveKind::Forward}));
    P0.PerTurtle.push_back(PlanFor(3, {EMoveKind::Forward}));
    TurnPlan P1; P1.Player = 1;
    P1.PerTurtle.push_back(PlanFor(2, {EMoveKind::Forward}));
    auto R = ResolveTurn(B, P0, P1);
    REQUIRE(!IsAlive(R.FinalState, 1));
    REQUIRE(!IsAlive(R.FinalState, 2));
    REQUIRE(!IsAlive(R.FinalState, 3));
}

// Case 7: turtle into static obstacle — turtle destroyed, obstacle survives.
TEST_CASE(Collision, Case07_RamStaticObstacle) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Obstacles.push_back(MakeStatic(100, IntVec3(5, 5, 5)));
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(99, {EMoveKind::Wait}))); // unused plan; player 1 has no turtle.
    REQUIRE(!IsAlive(R.FinalState, 1));
    REQUIRE(IsAliveObstacle(R.FinalState, 100));
}

// Case 8: turtle moves into a hunter cell while hunter moves away — turtle survives.
TEST_CASE(Collision, Case08_TrailingIntoVacatedHunterCell) {
    BoardState B = BlankBoard();
    // Hunter at (5,5,5); turtle nearby for hunter to chase. Place a different turtle to chase
    // so hunter moves away from (5,5,5). Then a third actor (we use turtle 1) trails into the
    // hunter's old cell.
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));   // trailer.
    B.Turtles.push_back(MakeTurtle(2, 0, IntVec3(7, 5, 5), EHeading::West));   // hunter target.
    B.Obstacles.push_back(MakeHunter(100, IntVec3(5, 5, 5)));
    // Hunter BFS: nearest turtle. Turtle 1 is at (4,5,5) (dist 1) and Turtle 2 at (7,5,5) (dist 2).
    // Hunter would step toward turtle 1 — into (4,5,5) — that's where T1 is leaving FROM. T1 moves
    // East into (5,5,5). Hunter moves West into (4,5,5) (T1's old cell). They effectively swap.
    // That's a swap collision: both destroyed. Not the case we want.
    //
    // Reframe: place T1 such that hunter moves AWAY from the cell T1 wants to enter.
    // Put T1 at (3,5,5) facing East (will move into (4,5,5)). Hunter at (5,5,5). Hunter's nearest
    // turtle is T1 at distance 2, hunter steps to (4,5,5). T1 also steps to (4,5,5) → both destroyed
    // by same-destination. Still not what we want.
    //
    // Build a scenario where the hunter target is far so the hunter steps in a different direction
    // than the trailer's path. Reset board:
    B.Turtles.clear(); B.Obstacles.clear();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));   // trailer.
    B.Turtles.push_back(MakeTurtle(2, 0, IntVec3(5, 8, 5), EHeading::South));  // hunter target to the +Y side.
    B.Obstacles.push_back(MakeHunter(100, IntVec3(5, 5, 5)));
    // Hunter's nearest turtle: T1 dist=1, T2 dist=3. Hunter will move toward T1 (lower id) —
    // but that's into (4,5,5), which is T1's origin (T1 moves out). Same-destination as T1's? No,
    // T1's destination is (5,5,5). Hunter's destination (4,5,5). T1's origin == hunter's destination
    // → swap-style? T1.From=(4,5,5), T1.To=(5,5,5), Hunter.From=(5,5,5), Hunter.To=(4,5,5).
    // T1.From == Hunter.To AND T1.To == Hunter.From — that IS a position swap. Both destroyed.
    //
    // To get a clean trailer test, give the trailer a different path: trailer at (4,5,5) heading
    // East (into (5,5,5)) and the hunter at (5,5,5) but with target only T1 — then hunter's BFS
    // step is toward (4,5,5), causing a swap. This setup forces a swap.
    //
    // Easier: trailer that doesn't end up in hunter's destination. T1 at (5,4,5) heading North,
    // moves into (5,5,5). Hunter at (5,5,5), nearest turtle T1 dist=1, step toward T1 → (5,4,5).
    // T1.From=(5,4,5), T1.To=(5,5,5), H.From=(5,5,5), H.To=(5,4,5). Position swap again.
    //
    // Conclusion: with one turtle-target, hunter always steps TOWARD that turtle, so they always
    // swap. The only "trailing into vacated hunter cell" requires multiple turtles where the
    // hunter chases a *different* one. Place T1 (trailer) and T2 (the actual target the hunter
    // chases away from T1's path):
    B.Turtles.clear(); B.Obstacles.clear();
    // Hunter at (5,5,5). T2 (lower id than trailer? we'll pick ids accordingly) closer in a
    // different direction. Trailer (the test subject) approaches the hunter cell along East.
    // Trailer T2 at (4,5,5), heading East -> wants (5,5,5).
    // Bait T1 at (5,5,8) heading South.
    // Hunter BFS: T1 at dist 3 (5,5,5)->(5,5,6)->(5,5,7)->(5,5,8). T2 at dist 1 (5,5,5)->(4,5,5).
    // Lower distance wins: hunter chases T2 — into (4,5,5), causing swap. Hmm.
    //
    // Make T1 closer than T2. T1 at (5,6,5) dist 1, T2 (trailer) at (4,5,5) dist 1.
    // Tie at dist 1. Lower TurtleId wins. If T1.Id=1, T2.Id=2, hunter chases T1: step into (5,6,5).
    // Trailer T2 steps into (5,5,5) (vacated hunter cell). T1 stays put (waiting).
    B.Turtles.push_back(MakeTurtle(1, 1, IntVec3(5, 6, 5), EHeading::North));  // bait, lowest id, opponent.
    B.Turtles.push_back(MakeTurtle(2, 0, IntVec3(4, 5, 5), EHeading::East));   // trailer.
    B.Obstacles.push_back(MakeHunter(100, IntVec3(5, 5, 5)));
    // Trailer plan: Forward.
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(2, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(1, {EMoveKind::Wait})));
    REQUIRE(IsAlive(R.FinalState, 2));
    REQUIRE(T(R.FinalState, 2)->Position == IntVec3(5, 5, 5));
}

// Case 9: forward off the grid edge — delivery intent.
TEST_CASE(Collision, Case09_ForwardOffEdgeIsDelivery) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(10, 5, 5), EHeading::East));
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(99, {})));
    const Turtle* X = R.FinalState.FindTurtle(1);
    REQUIRE(X != nullptr);
    REQUIRE(X->bDelivered);
}

// Case 10: delivery ram from behind destroys the deliverer before points score.
TEST_CASE(Collision, Case10_DeliveryRamFromBehind) {
    BoardState B = BlankBoard();
    // T1 about to deliver East off (10,5,5); hunter at (9,5,5) chasing T1.
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(10, 5, 5), EHeading::East));
    B.Turtles[0].CarriedPoints = 4;
    B.Obstacles.push_back(MakeHunter(100, IntVec3(9, 5, 5))); // hunter steps to (10,5,5).
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(99, {})));
    const Turtle* X = R.FinalState.FindTurtle(1);
    REQUIRE(X != nullptr);
    REQUIRE(!X->bAlive);
    REQUIRE(!X->bDelivered);
    REQUIRE(R.FinalState.ScorePlayer0 == 0);
}

TEST_CASE(Collision, Case10b_DestroyedRammerDoesNotCancelDelivery) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(10, 5, 5), EHeading::East));
    B.Turtles[0].CarriedPoints = 4;
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(10, 4, 5), EHeading::North)); // also aims for (10,5,5)
    B.Obstacles.push_back(MakeHunter(100, IntVec3(9, 5, 5)));                   // also aims for (10,5,5)

    TurnPlan P0; P0.Player = 0;
    P0.PerTurtle.push_back(PlanFor(1, {EMoveKind::Forward}));
    TurnPlan P1; P1.Player = 1;
    P1.PerTurtle.push_back(PlanFor(2, {EMoveKind::Forward}));

    auto R = ResolveTurn(B, P0, P1);
    const Turtle* Deliverer = R.FinalState.FindTurtle(1);
    REQUIRE(Deliverer != nullptr);
    REQUIRE(Deliverer->bDelivered);
    REQUIRE(R.FinalState.ScorePlayer0 == 4);
}

// Case 11: two turtles deliver off the same edge cell same tick — both deliver.
TEST_CASE(Collision, Case11_TwoDeliveriesSameCell) {
    // Hard to set up two turtles in the same cell preconditioned (the board generator wouldn't
    // produce that). Use case-spirit: two adjacent edge cells both delivering same tick.
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(10, 4, 5), EHeading::East));
    B.Turtles.push_back(MakeTurtle(2, 0, IntVec3(10, 6, 5), EHeading::East));
    TurnPlan P0; P0.Player = 0;
    P0.PerTurtle.push_back(PlanFor(1, {EMoveKind::Forward}));
    P0.PerTurtle.push_back(PlanFor(2, {EMoveKind::Forward}));
    auto R = ResolveTurn(B, P0, SinglePlan(1, PlanFor(99, {})));
    REQUIRE(R.FinalState.FindTurtle(1)->bDelivered);
    REQUIRE(R.FinalState.FindTurtle(2)->bDelivered);
}

// Case 12: hunter and turtle swap — both destroyed.
TEST_CASE(Collision, Case12_HunterTurtleSwap) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Obstacles.push_back(MakeHunter(100, IntVec3(5, 5, 5))); // chases T1, steps to (4,5,5).
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(99, {})));
    REQUIRE(!IsAlive(R.FinalState, 1));
    REQUIRE(!IsAliveObstacle(R.FinalState, 100));
}

// Case 13: stationary turtle hit by another moving in — stationary destroyed.
TEST_CASE(Collision, Case13_StationaryHit) {
    // Same as Case 3, with explicit Wait by the stationary.
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(5, 5, 5), EHeading::North));
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Wait})));
    REQUIRE(!IsAlive(R.FinalState, 2));
}

// Case 14: two turtles waiting on adjacent cells — nothing happens.
TEST_CASE(Collision, Case14_TwoWaiting) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(5, 5, 5), EHeading::East));
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(6, 5, 5), EHeading::West));
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Wait})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Wait})));
    REQUIRE(IsAlive(R.FinalState, 1));
    REQUIRE(IsAlive(R.FinalState, 2));
    REQUIRE(T(R.FinalState, 1)->Position == IntVec3(5, 5, 5));
    REQUIRE(T(R.FinalState, 2)->Position == IntVec3(6, 5, 5));
}

// ---- Pickup + delivery with carried points -------------------------------

TEST_CASE(Resolver, PickupAddsToCarriedPoints) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    Objective O; O.Id = 50; O.Position = IntVec3(5, 5, 5); O.PointValue = 3;
    B.Objectives.push_back(O);
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(99, {})));
    REQUIRE(R.FinalState.FindTurtle(1)->CarriedPoints == 3);
    REQUIRE(R.FinalState.FindObjective(50)->bClaimed);
    REQUIRE(R.FinalState.ScorePlayer0 == 0); // not delivered yet.
}

TEST_CASE(Resolver, DeliveryScoresCarriedPoints) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(9, 5, 5), EHeading::East));
    B.Turtles[0].CarriedPoints = 7;
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward, EMoveKind::Forward})),
        SinglePlan(1, PlanFor(99, {})));
    REQUIRE(R.FinalState.FindTurtle(1)->bDelivered);
    REQUIRE(R.FinalState.ScorePlayer0 == 7);
    REQUIRE(R.FinalState.FindTurtle(1)->CarriedPoints == 0);
}

TEST_CASE(Resolver, EmptyTurnStillAdvancesTurnWithoutHunterMove) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Obstacles.push_back(MakeHunter(100, IntVec3(8, 5, 5)));

    auto R = ResolveTurn(B, TurnPlan{}, TurnPlan{});

    REQUIRE(R.Ticks.empty());
    REQUIRE(R.FinalState.TurnNumber == 1);
    REQUIRE(R.FinalState.FindObstacle(100)->Position == IntVec3(8, 5, 5));
}

TEST_CASE(Resolver, DestroyedTurtleForfeitsCarriedPoints) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(6, 5, 5), EHeading::West));
    B.Turtles[0].CarriedPoints = 10;
    auto R = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Forward})));
    REQUIRE(!IsAlive(R.FinalState, 1));
    REQUIRE(R.FinalState.ScorePlayer0 == 0);
}

// ---- Determinism on a small case -----------------------------------------

TEST_CASE(Resolver, SameInputProducesIdenticalEvents) {
    BoardState B = BlankBoard();
    B.Turtles.push_back(MakeTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Turtles.push_back(MakeTurtle(2, 1, IntVec3(7, 5, 5), EHeading::West));
    B.Obstacles.push_back(MakeHunter(100, IntVec3(5, 6, 5)));

    auto R1 = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward, EMoveKind::YawLeft, EMoveKind::Forward})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Forward, EMoveKind::Forward, EMoveKind::Wait})));
    auto R2 = ResolveTurn(B,
        SinglePlan(0, PlanFor(1, {EMoveKind::Forward, EMoveKind::YawLeft, EMoveKind::Forward})),
        SinglePlan(1, PlanFor(2, {EMoveKind::Forward, EMoveKind::Forward, EMoveKind::Wait})));

    REQUIRE(R1.Ticks.size() == R2.Ticks.size());
    for (std::size_t i = 0; i < R1.Ticks.size(); ++i) {
        REQUIRE(R1.Ticks[i].Events.size() == R2.Ticks[i].Events.size());
        for (std::size_t j = 0; j < R1.Ticks[i].Events.size(); ++j) {
            const auto& A = R1.Ticks[i].Events[j];
            const auto& B2 = R2.Ticks[i].Events[j];
            REQUIRE(A.Type == B2.Type);
            REQUIRE(A.PrimaryId == B2.PrimaryId);
            REQUIRE(A.SecondaryId == B2.SecondaryId);
            REQUIRE(A.FromPosition == B2.FromPosition);
            REQUIRE(A.ToPosition == B2.ToPosition);
            REQUIRE(A.PointsDelta == B2.PointsDelta);
        }
    }
}

#endif
