#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GatherAndHuntPuzzleSession.h"

namespace
{
	ULevelSave* MakeBasePuzzle(const FString& LevelName)
	{
		ULevelSave* const LevelSave = NewObject<ULevelSave>(GetTransientPackage());
		LevelSave->LevelName = LevelName;
		LevelSave->GridSize = FVector(300.0f, 300.0f, 300.0f);
		LevelSave->GridCellSize = 100.0f;
		return LevelSave;
	}

	FPieceSaveData MakePlayerPiece(const int32 PieceSaveID, const FVector& Position, const FRotator& Rotation)
	{
		FPieceSaveData Data;
		Data.PieceSaveID = PieceSaveID;
		Data.PieceOwnerID = 0;
		Data.PieceType = PieceTypeEnum::BP_PlayerOwnedPiece;
		Data.Position = Position;
		Data.Rotation = Rotation;
		return Data;
	}

	FPieceSaveData MakeObjectivePiece(const int32 PieceSaveID, const FVector& Position, const PieceTypeEnum PieceType = PieceTypeEnum::BP_Objective)
	{
		FPieceSaveData Data;
		Data.PieceSaveID = PieceSaveID;
		Data.PieceType = PieceType;
		Data.Position = Position;
		return Data;
	}

	FPieceSaveData MakeLinearHazard(const int32 PieceSaveID, const FVector& Position)
	{
		FPieceSaveData Data;
		Data.PieceSaveID = PieceSaveID;
		Data.PieceType = PieceTypeEnum::BP_LinearPatrolHazard;
		Data.MovementType = MovementTypeEnum::BP_AbsoluteMoveLinearPatrol;
		Data.Position = Position;
		Data.Rotation = FRotator::ZeroRotator;
		Data.HazardStepDistance = 100.0f;
		Data.HazardPrimaryStepCount = 1;
		return Data;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntPuzzleRuntimeLoadConvertsBoardTest,
	"GatherAndHunt.GatherAndHunt.PuzzleRuntime.LoadConvertsBoard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntPuzzleRuntimeLoadConvertsBoardTest::RunTest(const FString&)
{
	ULevelSave* const Puzzle = MakeBasePuzzle(TEXT("RuntimeBoard"));
	Puzzle->TargetScore = 5;
	Puzzle->PieceSaveDataArray.Add(MakePlayerPiece(1, FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator));
	Puzzle->PieceSaveDataArray.Add(MakeObjectivePiece(2, FVector(200.0f, 0.0f, 0.0f), PieceTypeEnum::BP_GoldObjective));
	Puzzle->PieceSaveDataArray.Add(MakeLinearHazard(3, FVector(100.0f, 0.0f, 0.0f)));

	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	TestTrue(TEXT("Puzzle loads into a runtime session"), Session->LoadPuzzleFromLevelSave(Puzzle));

	const FGatherAndHuntPuzzleBoardSnapshot Snapshot = Session->GetCurrentBoardSnapshot();
	TestEqual(TEXT("Board size converts from world dimensions"), Snapshot.GridSize, FIntVector(4, 4, 4));
	TestEqual(TEXT("One turtle converted"), Snapshot.Turtles.Num(), 1);
	TestEqual(TEXT("One objective converted"), Snapshot.Objectives.Num(), 1);
	TestEqual(TEXT("One obstacle converted"), Snapshot.Obstacles.Num(), 1);
	TestEqual(TEXT("Gold objective keeps its point value"), Snapshot.Objectives[0].PointValue, 5);
	TestEqual(TEXT("Deterministic hazard converts to wandering obstacle"), Snapshot.Obstacles[0].ObstacleKind, static_cast<int32>(turtlesim::EObstacleKind::Wandering));
	TestEqual(TEXT("Linear hazard uses linear patrol pattern"), Snapshot.Obstacles[0].PatternKind, static_cast<int32>(turtlesim::EWanderingPattern::LinearPatrol));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntPuzzleRuntimeHazardResolutionTest,
	"GatherAndHunt.GatherAndHunt.PuzzleRuntime.HazardResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntPuzzleRuntimeHazardResolutionTest::RunTest(const FString&)
{
	ULevelSave* const Puzzle = MakeBasePuzzle(TEXT("HazardKill"));
	Puzzle->PieceSaveDataArray.Add(MakePlayerPiece(1, FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator));
	Puzzle->PieceSaveDataArray.Add(MakeLinearHazard(2, FVector(0.0f, 0.0f, 0.0f)));

	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	TestTrue(TEXT("Puzzle loads"), Session->LoadPuzzleFromLevelSave(Puzzle));

	FGatherAndHuntPuzzleTurnPlan TurnPlan;
	FGatherAndHuntPuzzleTurtlePlan TurtlePlan;
	TurtlePlan.TurtleId = 1;
	TurtlePlan.Moves.Add(EGatherAndHuntPuzzleMoveKind::Wait);
	TurnPlan.TurtlePlans.Add(TurtlePlan);

	FGatherAndHuntPuzzlePlanValidation Validation;
	TestTrue(TEXT("Turn plan resolves"), Session->SubmitSoloTurnPlan(TurnPlan, Validation));
	TestTrue(TEXT("Validation remains valid"), Validation.bIsValid);
	TestEqual(TEXT("Hazard impact defeats the puzzle"), Session->GetOutcome(), EGatherAndHuntPuzzleOutcome::Defeat);

	const FGatherAndHuntPuzzleBoardSnapshot Snapshot = Session->GetCurrentBoardSnapshot();
	TestFalse(TEXT("Waiting turtle is destroyed by the moving hazard"), Snapshot.Turtles[0].bAlive);
	TestEqual(TEXT("Hazard advanced to the turtle cell"), Snapshot.Obstacles[0].GridPosition, FIntVector(1, 0, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntPuzzleRuntimeVictoryFlowTest,
	"GatherAndHunt.GatherAndHunt.PuzzleRuntime.VictoryFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntPuzzleRuntimeVictoryFlowTest::RunTest(const FString&)
{
	ULevelSave* const Puzzle = MakeBasePuzzle(TEXT("VictoryFlow"));
	Puzzle->TargetScore = 1;
	Puzzle->TargetTurnCount = 3;
	Puzzle->PieceSaveDataArray.Add(MakePlayerPiece(1, FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f)));
	Puzzle->PieceSaveDataArray.Add(MakeObjectivePiece(2, FVector(0.0f, 0.0f, 0.0f)));

	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	TestTrue(TEXT("Puzzle loads"), Session->LoadPuzzleFromLevelSave(Puzzle));

	FGatherAndHuntPuzzleTurnPlan ClaimTurn;
	FGatherAndHuntPuzzleTurtlePlan ClaimTurtlePlan;
	ClaimTurtlePlan.TurtleId = 1;
	ClaimTurtlePlan.Moves.Add(EGatherAndHuntPuzzleMoveKind::Wait);
	ClaimTurn.TurtlePlans.Add(ClaimTurtlePlan);

	FGatherAndHuntPuzzlePlanValidation Validation;
	TestTrue(TEXT("First turn claims the objective"), Session->SubmitSoloTurnPlan(ClaimTurn, Validation));
	TestEqual(TEXT("Puzzle remains in progress after pickup"), Session->GetOutcome(), EGatherAndHuntPuzzleOutcome::InProgress);

	FGatherAndHuntPuzzleTurnPlan DeliverTurn;
	FGatherAndHuntPuzzleTurtlePlan DeliverTurtlePlan;
	DeliverTurtlePlan.TurtleId = 1;
	DeliverTurtlePlan.Moves.Add(EGatherAndHuntPuzzleMoveKind::Forward);
	DeliverTurn.TurtlePlans.Add(DeliverTurtlePlan);

	TestTrue(TEXT("Second turn delivers the carried point"), Session->SubmitSoloTurnPlan(DeliverTurn, Validation));
	TestEqual(TEXT("Target score triggers victory"), Session->GetOutcome(), EGatherAndHuntPuzzleOutcome::Victory);
	TestEqual(TEXT("Player score reflects the delivery"), Session->GetPlayerScore(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntPuzzleRuntimePlaybackDataTest,
	"GatherAndHunt.GatherAndHunt.PuzzleRuntime.PlaybackData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntPuzzleRuntimePlaybackDataTest::RunTest(const FString&)
{
	ULevelSave* const Puzzle = MakeBasePuzzle(TEXT("PlaybackData"));
	Puzzle->PieceSaveDataArray.Add(MakePlayerPiece(1, FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator));

	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	TestTrue(TEXT("Puzzle loads"), Session->LoadPuzzleFromLevelSave(Puzzle));

	FGatherAndHuntPuzzleTurnPlan TurnPlan;
	FGatherAndHuntPuzzleTurtlePlan TurtlePlan;
	TurtlePlan.TurtleId = 1;
	TurtlePlan.Moves.Add(EGatherAndHuntPuzzleMoveKind::YawLeft);
	TurnPlan.TurtlePlans.Add(TurtlePlan);

	FGatherAndHuntPuzzlePlanValidation Validation;
	TestTrue(TEXT("Turn resolves"), Session->SubmitSoloTurnPlan(TurnPlan, Validation));

	const FGatherAndHuntPuzzleBoardSnapshot StartSnapshot = Session->GetLastPlaybackStartBoardSnapshot();
	const FGatherAndHuntPuzzleTurnResultView PlaybackView = Session->GetLastTurnResultView();

	TestEqual(TEXT("Playback start snapshot preserves the pre-turn turtle position"), StartSnapshot.Turtles[0].GridPosition, FIntVector(0, 0, 0));
	TestEqual(TEXT("Playback captures exactly one tick"), PlaybackView.Ticks.Num(), 1);
	TestEqual(TEXT("Tick captures exactly one event"), PlaybackView.Ticks[0].Events.Num(), 1);
	TestEqual(TEXT("Playback event records the starting heading"), PlaybackView.Ticks[0].Events[0].FromHeading, 1);
	TestEqual(TEXT("Playback event records the ending heading"), PlaybackView.Ticks[0].Events[0].ToHeading, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntPuzzleRuntimeScriptedOpponentTurnTest,
	"GatherAndHunt.GatherAndHunt.PuzzleRuntime.ScriptedOpponentTurn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntPuzzleRuntimeScriptedOpponentTurnTest::RunTest(const FString&)
{
	ULevelSave* const Puzzle = MakeBasePuzzle(TEXT("ScriptedOpponent"));
	Puzzle->TargetTurnCount = 2;

	FPieceSaveData PlayerPiece = MakePlayerPiece(1, FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	PlayerPiece.PieceOwnerID = 0;
	Puzzle->PieceSaveDataArray.Add(PlayerPiece);

	FPieceSaveData OpponentPiece = MakePlayerPiece(2, FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	OpponentPiece.PieceOwnerID = 1;
	Puzzle->PieceSaveDataArray.Add(OpponentPiece);

	FScriptedOpponentTurnData OpponentTurn;
	OpponentTurn.TurnNumber = 0;
	FScriptedPuzzleTurtlePlanData OpponentTurtlePlan;
	OpponentTurtlePlan.TurtleId = 2;
	OpponentTurtlePlan.Moves.Add(EGatherAndHuntPuzzleMoveKind::Forward);
	OpponentTurn.TurtlePlans.Add(OpponentTurtlePlan);
	Puzzle->ScriptedOpponentTurns.Add(OpponentTurn);

	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	TestTrue(TEXT("Puzzle loads"), Session->LoadPuzzleFromLevelSave(Puzzle));

	FGatherAndHuntPuzzleTurnPlan PlayerTurn;
	FGatherAndHuntPuzzleTurtlePlan PlayerTurtlePlan;
	PlayerTurtlePlan.TurtleId = 1;
	PlayerTurtlePlan.Moves.Add(EGatherAndHuntPuzzleMoveKind::Wait);
	PlayerTurn.TurtlePlans.Add(PlayerTurtlePlan);

	FGatherAndHuntPuzzlePlanValidation Validation;
	TestTrue(TEXT("Player turn resolves against scripted opponent"), Session->SubmitSoloTurnPlan(PlayerTurn, Validation));

	const FGatherAndHuntPuzzleBoardSnapshot Snapshot = Session->GetCurrentBoardSnapshot();
	TestFalse(TEXT("Player turtle is destroyed by the scripted opponent move"), Snapshot.Turtles[0].bAlive);
	TestTrue(TEXT("Scripted opponent turtle survives the stationary hit"), Snapshot.Turtles[1].bAlive);
	TestEqual(TEXT("Scripted opponent reaches the player's cell"), Snapshot.Turtles[1].GridPosition, FIntVector(1, 0, 0));
	TestEqual(TEXT("No score is awarded"), Session->GetPlayerScore(), 0);
	return true;
}

#endif
