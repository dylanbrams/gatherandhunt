#include "TestFramework.h"
#include "PlanPreview.h"

#if defined(__UNREAL__)
// Standalone TurtleSim tests are excluded from Unreal module builds.
#else

using namespace turtlesim;

static BoardState PreviewBoard() {
    BoardState B;
    B.Size = IntVec3(11, 11, 11);
    return B;
}

static Turtle PreviewTurtle(TurtleId Id, PlayerId Owner, IntVec3 Pos, EHeading H) {
    Turtle T;
    T.Id = Id; T.Owner = Owner; T.Position = Pos;
    T.Facing = Orientation(H, EPitch::Level);
    return T;
}

static TurnPlan PreviewPlan(PlayerId Player, TurtleId Id, std::initializer_list<EMoveKind> Kinds) {
    TurnPlan P; P.Player = Player;
    TurtlePlan TP; TP.Turtle = Id;
    for (auto K : Kinds) TP.Moves.push_back(Move{K});
    P.PerTurtle.push_back(TP);
    return P;
}

TEST_CASE(PlanPreview, UnknownTurtleIsEmpty) {
    BoardState B = PreviewBoard();
    const TurnResult R = ResolveTurn(B, TurnPlan{}, TurnPlan{});
    REQUIRE(ExtractTurtleTrack(B, R, 7).empty());
}

TEST_CASE(PlanPreview, NoPlanIsJustStart) {
    BoardState B = PreviewBoard();
    B.Turtles.push_back(PreviewTurtle(1, 0, IntVec3(2, 2, 2), EHeading::East));
    const TurnResult R = ResolveTurn(B, TurnPlan{}, TurnPlan{});
    const auto Track = ExtractTurtleTrack(B, R, 1);
    REQUIRE(Track.size() == 1);
    REQUIRE(Track[0].Kind == ETrackStepKind::Start);
    REQUIRE(Track[0].Position == IntVec3(2, 2, 2));
}

TEST_CASE(PlanPreview, MoveTurnMoveFollowsHeading) {
    BoardState B = PreviewBoard();
    B.Turtles.push_back(PreviewTurtle(1, 0, IntVec3(2, 2, 2), EHeading::East));
    const TurnResult R = ResolveTurn(B,
        PreviewPlan(0, 1, {EMoveKind::Forward, EMoveKind::YawLeft, EMoveKind::Forward}),
        TurnPlan{});
    const auto Track = ExtractTurtleTrack(B, R, 1);
    REQUIRE(Track.size() == 4);
    REQUIRE(Track[1].Kind == ETrackStepKind::Moved);
    REQUIRE(Track[1].Position == IntVec3(3, 2, 2));
    REQUIRE(Track[2].Kind == ETrackStepKind::Turned);
    REQUIRE(Track[2].Position == IntVec3(3, 2, 2));
    REQUIRE(Track[2].Facing.Heading == EHeading::North);
    REQUIRE(Track[3].Position == IntVec3(3, 3, 2));
    REQUIRE(Track[3].TickIndex == 2);
}

TEST_CASE(PlanPreview, ClaimFoldsIntoMoveStep) {
    BoardState B = PreviewBoard();
    B.Turtles.push_back(PreviewTurtle(1, 0, IntVec3(2, 2, 2), EHeading::East));
    Objective O; O.Id = 50; O.Position = IntVec3(3, 2, 2); O.PointValue = 3;
    B.Objectives.push_back(O);
    const TurnResult R = ResolveTurn(B, PreviewPlan(0, 1, {EMoveKind::Forward}), TurnPlan{});
    const auto Track = ExtractTurtleTrack(B, R, 1);
    REQUIRE(Track.size() == 2);
    REQUIRE(Track[1].Kind == ETrackStepKind::Moved);
    REQUIRE(Track[1].bClaimedObjective);
    REQUIRE(Track[1].PointsDelta == 3);
}

TEST_CASE(PlanPreview, DeliveryPointsOffGrid) {
    BoardState B = PreviewBoard();
    B.Turtles.push_back(PreviewTurtle(1, 0, IntVec3(10, 5, 5), EHeading::East));
    B.Turtles[0].CarriedPoints = 5;
    const TurnResult R = ResolveTurn(B, PreviewPlan(0, 1, {EMoveKind::Forward}), TurnPlan{});
    const auto Track = ExtractTurtleTrack(B, R, 1);
    REQUIRE(Track.size() == 2);
    REQUIRE(Track[1].Kind == ETrackStepKind::Delivered);
    REQUIRE(Track[1].Position == IntVec3(11, 5, 5));
    REQUIRE(Track[1].PointsDelta == 5);
}

TEST_CASE(PlanPreview, StaticObstacleEndsTrackDestroyed) {
    BoardState B = PreviewBoard();
    B.Turtles.push_back(PreviewTurtle(1, 0, IntVec3(2, 2, 2), EHeading::East));
    Obstacle Wall; Wall.Id = 100; Wall.Kind = EObstacleKind::Static; Wall.Position = IntVec3(3, 2, 2);
    B.Obstacles.push_back(Wall);
    const TurnResult R = ResolveTurn(B,
        PreviewPlan(0, 1, {EMoveKind::Forward, EMoveKind::Forward}), TurnPlan{});
    const auto Track = ExtractTurtleTrack(B, R, 1);
    REQUIRE(Track.back().Kind == ETrackStepKind::Destroyed);
    // Nothing after death: the second Forward never executes.
    REQUIRE(Track.size() == 2);
}

TEST_CASE(PlanPreview, HeadOnWithOpponentShowsDestroyed) {
    BoardState B = PreviewBoard();
    B.Turtles.push_back(PreviewTurtle(1, 0, IntVec3(4, 5, 5), EHeading::East));
    B.Turtles.push_back(PreviewTurtle(2, 1, IntVec3(6, 5, 5), EHeading::West));
    const TurnResult R = ResolveTurn(B,
        PreviewPlan(0, 1, {EMoveKind::Forward}),
        PreviewPlan(1, 2, {EMoveKind::Forward}));
    const auto Track = ExtractTurtleTrack(B, R, 1);
    REQUIRE(Track.size() == 2);
    REQUIRE(Track[1].Kind == ETrackStepKind::Destroyed);
}

#endif
