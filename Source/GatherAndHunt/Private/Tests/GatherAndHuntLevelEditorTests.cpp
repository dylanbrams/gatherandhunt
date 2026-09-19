#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GatherAndHuntLevelEditorLibrary.h"

namespace
{
	FPieceSaveData MakeObjectivePiece(const int32 PieceSaveID, const FVector Position)
	{
		FPieceSaveData Data;
		Data.PieceSaveID = PieceSaveID;
		Data.Position = Position;
		Data.PieceType = PieceTypeEnum::BP_Objective;
		return Data;
	}

	FPieceSaveData MakeCircularHazardPiece(const int32 PieceSaveID, const FVector Position)
	{
		FPieceSaveData Data;
		Data.PieceSaveID = PieceSaveID;
		Data.Position = Position;
		Data.PieceType = PieceTypeEnum::BP_CircularHazard;
		Data.MovementType = MovementTypeEnum::BP_AbsoluteMoveCircle;
		Data.HazardStepDistance = 100.0f;
		Data.HazardPrimaryStepCount = 2;
		Data.HazardSecondaryStepCount = 1;
		return Data;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntLevelEditorValidateLevelSaveTest,
	"GatherAndHunt.GatherAndHunt.LevelEditor.ValidateLevelSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntLevelEditorValidateLevelSaveTest::RunTest(const FString&)
{
	ULevelSave* const LevelSave = UGatherAndHuntLevelEditorLibrary::CreateEmptyLevelSave(
		GetTransientPackage(),
		TEXT("ValidationBoard"),
		9,
		FVector(500.0f, 500.0f, 500.0f));
	LevelSave->TargetObjectiveCount = 1;

	UGatherAndHuntLevelEditorLibrary::UpsertPieceData(LevelSave, MakeObjectivePiece(0, FVector(100.0f, 100.0f, 100.0f)));
	UGatherAndHuntLevelEditorLibrary::UpsertPieceData(LevelSave, MakeCircularHazardPiece(1, FVector(200.0f, 200.0f, 0.0f)));

	TArray<FLevelEditorValidationIssue> Issues;
	const bool bIsValid = UGatherAndHuntLevelEditorLibrary::ValidateLevelSave(LevelSave, Issues);

	TestTrue(TEXT("A valid authored level passes validation"), bIsValid);
	TestEqual(TEXT("No validation issues for valid level"), Issues.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntLevelEditorRejectsBadHazardsTest,
	"GatherAndHunt.GatherAndHunt.LevelEditor.RejectsBadHazards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntLevelEditorRejectsBadHazardsTest::RunTest(const FString&)
{
	ULevelSave* const LevelSave = UGatherAndHuntLevelEditorLibrary::CreateEmptyLevelSave(
		GetTransientPackage(),
		TEXT("BrokenBoard"),
		10,
		FVector(200.0f, 200.0f, 200.0f));

	FPieceSaveData BrokenHazard = MakeCircularHazardPiece(5, FVector(250.0f, 0.0f, 0.0f));
	BrokenHazard.HazardStepDistance = 0.0f;
	BrokenHazard.HazardSecondaryStepCount = 0;
	LevelSave->PieceSaveDataArray.Add(BrokenHazard);

	TArray<FLevelEditorValidationIssue> Issues;
	const bool bIsValid = UGatherAndHuntLevelEditorLibrary::ValidateLevelSave(LevelSave, Issues);

	TestFalse(TEXT("Invalid hazard authoring fails validation"), bIsValid);
	TestTrue(TEXT("Validation reports multiple issues"), Issues.Num() >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntLevelEditorRoundTripTest,
	"GatherAndHunt.GatherAndHunt.LevelEditor.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntLevelEditorRoundTripTest::RunTest(const FString&)
{
	const FString RelativeFilename = TEXT("Automation/LevelEditorRoundTrip");
	ULevelSave* const LevelSave = UGatherAndHuntLevelEditorLibrary::CreateEmptyLevelSave(
		GetTransientPackage(),
		TEXT("RoundTripBoard"),
		11,
		FVector(300.0f, 400.0f, 500.0f));
	LevelSave->TargetObjectiveCount = 1;
	LevelSave->TargetTurnCount = 8;
	LevelSave->TargetScore = 6;
	LevelSave->PuzzleAct = 3;
	LevelSave->PuzzleOrder = 4;
	LevelSave->IntroText = TEXT("Round-trip this puzzle metadata.");
	LevelSave->GridCellSize = 50.0f;

	UGatherAndHuntLevelEditorLibrary::UpsertPieceData(LevelSave, MakeObjectivePiece(0, FVector(100.0f, 0.0f, 0.0f)));

	FPieceSaveData Hazard = MakeCircularHazardPiece(1, FVector(0.0f, 100.0f, 0.0f));
	Hazard.HazardPhaseOffset = 3;
	UGatherAndHuntLevelEditorLibrary::UpsertPieceData(LevelSave, Hazard);

	FScriptedOpponentTurnData OpponentTurn;
	OpponentTurn.TurnNumber = 0;
	FScriptedPuzzleTurtlePlanData OpponentTurtlePlan;
	OpponentTurtlePlan.TurtleId = 99;
	OpponentTurtlePlan.Moves.Add(EGatherAndHuntPuzzleMoveKind::Forward);
	OpponentTurn.TurtlePlans.Add(OpponentTurtlePlan);
	LevelSave->ScriptedOpponentTurns.Add(OpponentTurn);

	TestTrue(TEXT("Saving level authoring data succeeds"),
		UGatherAndHuntLevelEditorLibrary::SaveLevelSaveToProjectFile(LevelSave, RelativeFilename));

	ULevelSave* const LoadedLevelSave = UGatherAndHuntLevelEditorLibrary::LoadLevelSaveFromProjectFile(
		GetTransientPackage(),
		RelativeFilename);

	TestNotNull(TEXT("Loading saved level returns an object"), LoadedLevelSave);
	if (LoadedLevelSave == nullptr)
	{
		return false;
	}

	TestEqual(TEXT("Piece count survives round-trip"), LoadedLevelSave->PieceSaveDataArray.Num(), 2);
	TestEqual(TEXT("Grid size survives round-trip"), LoadedLevelSave->GridSize, FVector(300.0f, 400.0f, 500.0f));
	TestEqual(TEXT("Target turn count survives round-trip"), LoadedLevelSave->TargetTurnCount, 8);
	TestEqual(TEXT("Target score survives round-trip"), LoadedLevelSave->TargetScore, 6);
	TestEqual(TEXT("Puzzle act survives round-trip"), LoadedLevelSave->PuzzleAct, 3);
	TestEqual(TEXT("Puzzle order survives round-trip"), LoadedLevelSave->PuzzleOrder, 4);
	TestEqual(TEXT("Intro text survives round-trip"), LoadedLevelSave->IntroText, FString(TEXT("Round-trip this puzzle metadata.")));
	TestEqual(TEXT("Grid cell size survives round-trip"), LoadedLevelSave->GridCellSize, 50.0f);
	TestEqual(TEXT("Hazard phase offset survives round-trip"), LoadedLevelSave->PieceSaveDataArray[1].HazardPhaseOffset, 3);
	TestEqual(TEXT("Scripted opponent turn count survives round-trip"), LoadedLevelSave->ScriptedOpponentTurns.Num(), 1);
	TestEqual(TEXT("Scripted opponent turn number survives round-trip"), LoadedLevelSave->ScriptedOpponentTurns[0].TurnNumber, 0);
	TestEqual(TEXT("Scripted opponent turtle survives round-trip"), LoadedLevelSave->ScriptedOpponentTurns[0].TurtlePlans[0].TurtleId, 99);
	TestEqual(TEXT("Scripted opponent move survives round-trip"), LoadedLevelSave->ScriptedOpponentTurns[0].TurtlePlans[0].Moves[0], EGatherAndHuntPuzzleMoveKind::Forward);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntLevelEditorHazardPreviewPathTest,
	"GatherAndHunt.GatherAndHunt.LevelEditor.HazardPreviewPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntLevelEditorHazardPreviewPathTest::RunTest(const FString&)
{
	const FPieceSaveData Hazard = MakeCircularHazardPiece(3, FVector::ZeroVector);
	const TArray<FVector> PreviewPath = UGatherAndHuntLevelEditorLibrary::BuildHazardPreviewPath(Hazard, 0);

	TestEqual(TEXT("Preview path uses full deterministic cycle by default"), PreviewPath.Num(), 6);
	TestEqual(TEXT("Preview path starts at authored origin"), PreviewPath[0], FVector::ZeroVector);
	TestEqual(TEXT("Preview path shows the second sample"), PreviewPath[1], FVector(0.0f, 100.0f, 0.0f));
	return true;
}

#endif
