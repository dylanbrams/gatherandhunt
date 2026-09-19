#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Match/TurtleGameState.h"
#include "Net/TurtleSerializer.h"

namespace
{
	using namespace turtlesim;

	static bool EqualIntVec3(const IntVec3& A, const IntVec3& B)
	{
		return A == B;
	}

	static bool EqualOrientation(const Orientation& A, const Orientation& B)
	{
		return A.Heading == B.Heading && A.Pitch == B.Pitch;
	}

	static bool EqualBoardState(const BoardState& A, const BoardState& B)
	{
		if (!EqualIntVec3(A.Size, B.Size)) return false;
		if (A.TurnNumber != B.TurnNumber) return false;
		if (A.ScorePlayer0 != B.ScorePlayer0 || A.ScorePlayer1 != B.ScorePlayer1) return false;
		if (A.Seed != B.Seed) return false;
		if (A.Turtles.size() != B.Turtles.size()) return false;
		if (A.Obstacles.size() != B.Obstacles.size()) return false;
		if (A.Objectives.size() != B.Objectives.size()) return false;

		for (size_t i = 0; i < A.Turtles.size(); ++i)
		{
			const Turtle& TA = A.Turtles[i];
			const Turtle& TB = B.Turtles[i];
			if (TA.Id != TB.Id || TA.Owner != TB.Owner) return false;
			if (!EqualIntVec3(TA.Position, TB.Position)) return false;
			if (!EqualOrientation(TA.Facing, TB.Facing)) return false;
			if (TA.bAlive != TB.bAlive || TA.bDelivered != TB.bDelivered) return false;
			if (TA.CarriedPoints != TB.CarriedPoints) return false;
		}

		for (size_t i = 0; i < A.Obstacles.size(); ++i)
		{
			const Obstacle& OA = A.Obstacles[i];
			const Obstacle& OB = B.Obstacles[i];
			if (OA.Id != OB.Id || OA.Kind != OB.Kind || OA.Pattern != OB.Pattern) return false;
			if (!EqualIntVec3(OA.Position, OB.Position)) return false;
			if (!EqualIntVec3(OA.OriginPosition, OB.OriginPosition)) return false;
			if (!EqualOrientation(OA.PatternOrientation, OB.PatternOrientation)) return false;
			if (OA.StepDistance != OB.StepDistance) return false;
			if (OA.PrimaryStepCount != OB.PrimaryStepCount) return false;
			if (OA.SecondaryStepCount != OB.SecondaryStepCount) return false;
			if (OA.PhaseOffset != OB.PhaseOffset) return false;
			if (OA.bAlive != OB.bAlive) return false;
		}

		for (size_t i = 0; i < A.Objectives.size(); ++i)
		{
			const Objective& OA = A.Objectives[i];
			const Objective& OB = B.Objectives[i];
			if (OA.Id != OB.Id || OA.PointValue != OB.PointValue) return false;
			if (!EqualIntVec3(OA.Position, OB.Position)) return false;
			if (OA.bClaimed != OB.bClaimed) return false;
		}

		return true;
	}

	static bool EqualTickEvent(const TickEvent& A, const TickEvent& B)
	{
		return A.Type == B.Type
			&& A.PrimaryId == B.PrimaryId
			&& A.SecondaryId == B.SecondaryId
			&& A.PointsDelta == B.PointsDelta
			&& EqualIntVec3(A.FromPosition, B.FromPosition)
			&& EqualIntVec3(A.ToPosition, B.ToPosition)
			&& EqualOrientation(A.FromOrientation, B.FromOrientation)
			&& EqualOrientation(A.ToOrientation, B.ToOrientation);
	}

	static bool EqualTurnResult(const TurnResult& A, const TurnResult& B)
	{
		if (!EqualBoardState(A.FinalState, B.FinalState)) return false;
		if (A.Ticks.size() != B.Ticks.size()) return false;
		for (size_t i = 0; i < A.Ticks.size(); ++i)
		{
			if (A.Ticks[i].TickIndex != B.Ticks[i].TickIndex) return false;
			if (A.Ticks[i].Events.size() != B.Ticks[i].Events.size()) return false;
			for (size_t j = 0; j < A.Ticks[i].Events.size(); ++j)
			{
				if (!EqualTickEvent(A.Ticks[i].Events[j], B.Ticks[i].Events[j])) return false;
			}
		}
		return true;
	}

	static BoardState MakeBoardState()
	{
		BoardState B;
		B.Size = IntVec3(11, 11, 11);
		B.TurnNumber = 3;
		B.ScorePlayer0 = 4;
		B.ScorePlayer1 = 7;
		B.Seed = 12345;

		Turtle T0;
		T0.Id = 1;
		T0.Owner = 0;
		T0.Position = IntVec3(2, 3, 4);
		T0.Facing = Orientation(EHeading::East, EPitch::Level);
		T0.CarriedPoints = 5;
		B.Turtles.push_back(T0);

		Turtle T1;
		T1.Id = 2;
		T1.Owner = 1;
		T1.Position = IntVec3(8, 6, 5);
		T1.Facing = Orientation(EHeading::North, EPitch::Up);
		T1.bAlive = false;
		T1.bDelivered = true;
		B.Turtles.push_back(T1);

		Obstacle O;
		O.Id = 100;
		O.Kind = EObstacleKind::Wandering;
		O.Pattern = EWanderingPattern::LinearPatrol;
		O.Position = IntVec3(5, 5, 5);
		O.OriginPosition = IntVec3(4, 5, 5);
		O.PatternOrientation = Orientation(EHeading::East, EPitch::Level);
		O.StepDistance = 1;
		O.PrimaryStepCount = 2;
		O.SecondaryStepCount = 1;
		O.PhaseOffset = 1;
		B.Obstacles.push_back(O);

		Objective J;
		J.Id = 200;
		J.Position = IntVec3(7, 7, 7);
		J.PointValue = 3;
		J.bClaimed = true;
		B.Objectives.push_back(J);

		return B;
	}

	static TurnResult MakeTurnResult()
	{
		TurnResult R;
		R.FinalState = MakeBoardState();

		TickResult Tick;
		Tick.TickIndex = 0;

		TickEvent E0;
		E0.Type = EEventType::TurtleMoved;
		E0.PrimaryId = 1;
		E0.FromPosition = IntVec3(1, 3, 4);
		E0.ToPosition = IntVec3(2, 3, 4);
		E0.FromOrientation = Orientation(EHeading::East, EPitch::Level);
		E0.ToOrientation = Orientation(EHeading::East, EPitch::Level);
		Tick.Events.push_back(E0);

		TickEvent E1;
		E1.Type = EEventType::TurtleDelivered;
		E1.PrimaryId = 2;
		E1.FromPosition = IntVec3(10, 6, 5);
		E1.ToPosition = IntVec3(10, 6, 5);
		E1.PointsDelta = 7;
		Tick.Events.push_back(E1);

		R.Ticks.push_back(Tick);
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurtleGameBoardStateRoundTripTest,
	"GatherAndHunt.TurtleGame.Serializer.BoardStateRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurtleGameBoardStateRoundTripTest::RunTest(const FString&)
{
	const BoardState Original = MakeBoardState();
	TArray<uint8> Bytes;
	TurtleSimWire::SerializeBoardState(Bytes, Original);

	BoardState RoundTripped;
	TestTrue(TEXT("DeserializeBoardState succeeds"), TurtleSimWire::DeserializeBoardState(Bytes, RoundTripped));
	TestTrue(TEXT("BoardState survives round trip"), EqualBoardState(Original, RoundTripped));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurtleGameTurnResultRoundTripTest,
	"GatherAndHunt.TurtleGame.Serializer.TurnResultRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurtleGameTurnResultRoundTripTest::RunTest(const FString&)
{
	const TurnResult Original = MakeTurnResult();
	TArray<uint8> Bytes;
	TurtleSimWire::SerializeTurnResult(Bytes, Original);

	TurnResult RoundTripped;
	TestTrue(TEXT("DeserializeTurnResult succeeds"), TurtleSimWire::DeserializeTurnResult(Bytes, RoundTripped));
	TestTrue(TEXT("TurnResult survives round trip"), EqualTurnResult(Original, RoundTripped));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurtleGameStateApplyFullBoardStateTest,
	"GatherAndHunt.TurtleGame.GameState.ApplyFullBoardState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurtleGameStateApplyFullBoardStateTest::RunTest(const FString&)
{
	ATurtleGameState* GS = NewObject<ATurtleGameState>();
	const BoardState Board = MakeBoardState();
	const TurnResult Result = MakeTurnResult();

	GS->ApplyTurnResult(Result);
	TestTrue(TEXT("Turn result cache primed"), GS->HasLastReceivedTurnResult());

	GS->ApplyFullBoardState(Board);
	TestTrue(TEXT("Board cache populated"), GS->HasCachedBoardState());
	TestFalse(TEXT("Full board state clears stale turn result"), GS->HasLastReceivedTurnResult());
	TestTrue(TEXT("Cached board matches input"), EqualBoardState(*GS->GetCachedBoardState(), Board));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurtleGameStateApplyTurnResultTest,
	"GatherAndHunt.TurtleGame.GameState.ApplyTurnResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurtleGameStateApplyTurnResultTest::RunTest(const FString&)
{
	ATurtleGameState* GS = NewObject<ATurtleGameState>();
	const TurnResult Result = MakeTurnResult();

	GS->ApplyTurnResult(Result);
	TestTrue(TEXT("Turn result cache populated"), GS->HasLastReceivedTurnResult());
	TestTrue(TEXT("Board cache populated from turn result"), GS->HasCachedBoardState());
	TestTrue(TEXT("Cached final board matches turn result"), EqualBoardState(*GS->GetCachedBoardState(), Result.FinalState));
	TestTrue(TEXT("Cached turn result matches input"), EqualTurnResult(*GS->GetLastReceivedTurnResult(), Result));
	return true;
}

#endif
