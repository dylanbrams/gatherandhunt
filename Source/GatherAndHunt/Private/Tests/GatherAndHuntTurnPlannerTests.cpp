#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GatherAndHuntLevelEditorLibrary.h"
#include "GatherAndHuntPuzzleSession.h"
#include "GatherAndHuntTurnPlanner.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
	FPieceSaveData MakePlannerPiece(const int32 Id, const PieceTypeEnum Type, const FVector& Position)
	{
		FPieceSaveData Data;
		Data.PieceSaveID = Id;
		Data.PieceOwnerID = 0;
		Data.PieceType = Type;
		Data.Position = Position;
		Data.Rotation = FRotator::ZeroRotator; // Faces +X (East).
		return Data;
	}

	// 5x5x5 board: turtle 1 at (0,0,0) facing East, a 1-point objective two cells ahead.
	UGatherAndHuntTurnPlanner* MakePlanner(UGatherAndHuntPuzzleSession*& OutSession, bool bWithHazard = false)
	{
		ULevelSave* const Puzzle = NewObject<ULevelSave>(GetTransientPackage());
		Puzzle->LevelName = TEXT("PlannerTest");
		Puzzle->GridSize = FVector(400.0f);
		Puzzle->GridCellSize = 100.0f;
		Puzzle->TargetScore = 100;
		Puzzle->PieceSaveDataArray.Add(MakePlannerPiece(1, PieceTypeEnum::BP_PlayerOwnedPiece, FVector::ZeroVector));
		Puzzle->PieceSaveDataArray.Add(MakePlannerPiece(2, PieceTypeEnum::BP_Objective, FVector(200.0f, 0.0f, 0.0f)));
		if (bWithHazard)
		{
			FPieceSaveData Hazard = MakePlannerPiece(3, PieceTypeEnum::BP_LinearPatrolHazard, FVector(0.0f, 200.0f, 0.0f));
			Hazard.MovementType = MovementTypeEnum::BP_AbsoluteMoveLinearPatrol;
			Hazard.Rotation = FRotator(0.0f, -90.0f, 0.0f); // Patrols toward -Y, into the turtle's column.
			Hazard.HazardStepDistance = 100.0f;
			Hazard.HazardPrimaryStepCount = 2;
			Puzzle->PieceSaveDataArray.Add(Hazard);
		}

		OutSession = NewObject<UGatherAndHuntPuzzleSession>();
		OutSession->LoadPuzzleFromLevelSave(Puzzle);

		UGatherAndHuntTurnPlanner* const Planner = NewObject<UGatherAndHuntTurnPlanner>();
		Planner->BindToSession(OutSession);
		return Planner;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntTurnPlannerEditingTest,
	"GatherAndHunt.GatherAndHunt.Planner.EditingAndCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntTurnPlannerEditingTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* Session = nullptr;
	UGatherAndHuntTurnPlanner* const Planner = MakePlanner(Session);

	TestTrue(TEXT("Planner sees the loaded board"), Planner->HasBoard());
	TestEqual(TEXT("Only turtle is auto-selected"), Planner->GetSelectedTurtleId(), 1);
	TestTrue(TEXT("Empty plan is submittable"), Planner->CanSubmit());

	for (int32 Index = 0; Index < 7; ++Index)
	{
		Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
	}
	TestEqual(TEXT("Seven moves cost 6*1 + 1*2"), Planner->GetTotalCost(), 8);
	TestEqual(TEXT("Summary reports tier 2"), Planner->GetTurtleSummaries()[0].HighestCostTier, 2);
	TestEqual(TEXT("Appending leaves the cursor at the end"), Planner->GetSelectedMoveIndex(), static_cast<int32>(INDEX_NONE));

	Planner->SelectMove(0);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Forward);
	TArray<EGatherAndHuntPuzzleMoveKind> Moves = Planner->GetTurtleMoves(1);
	TestEqual(TEXT("Insert lands after the selected move"), Moves[1], EGatherAndHuntPuzzleMoveKind::Forward);
	TestEqual(TEXT("Inserted move becomes selected"), Planner->GetSelectedMoveIndex(), 1);

	Planner->ShiftSelectedMove(-1);
	Moves = Planner->GetTurtleMoves(1);
	TestEqual(TEXT("Shift left moves the move to the front"), Moves[0], EGatherAndHuntPuzzleMoveKind::Forward);
	TestEqual(TEXT("Selection follows the shifted move"), Planner->GetSelectedMoveIndex(), 0);

	Planner->RemoveSelectedMove();
	TestEqual(TEXT("Delete removes the selected move"), Planner->GetTurtleMoves(1).Num(), 7);
	TestEqual(TEXT("Deleted move was the forward"), Planner->GetTurtleMoves(1)[0], EGatherAndHuntPuzzleMoveKind::Wait);

	Planner->ClearAll();
	TestEqual(TEXT("Clear all empties the plan"), Planner->GetTotalCost(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntTurnPlannerValidationTest,
	"GatherAndHunt.GatherAndHunt.Planner.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntTurnPlannerValidationTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* Session = nullptr;
	UGatherAndHuntTurnPlanner* const Planner = MakePlanner(Session);

	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::PitchDown);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::PitchDown);
	FGatherAndHuntPuzzlePlanValidation Validation = Planner->GetValidation();
	TestFalse(TEXT("Pitching down twice from level is invalid"), Validation.bIsValid);
	TestEqual(TEXT("Second move is flagged"), Validation.OffendingMoveIndex, 1);
	TestFalse(TEXT("Invalid plan cannot be submitted"), Planner->CanSubmit());
	TestTrue(TEXT("Turtle summary carries the error"), Planner->GetTurtleSummaries()[0].bHasValidationError);

	FGatherAndHuntPuzzlePlanValidation SubmitResult;
	TestFalse(TEXT("Submit refuses an invalid plan"), Planner->Submit(SubmitResult));
	TestEqual(TEXT("Session did not advance"), Session->GetCurrentTurnNumber(), 0);

	Planner->ClearAll();
	for (int32 Index = 0; Index < 13; ++Index)
	{
		Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
	}
	TestEqual(TEXT("Thirteen moves cost 23"), Planner->GetTotalCost(), 23);
	TestFalse(TEXT("Over-budget plan is invalid"), Planner->GetValidation().bIsValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntTurnPlannerPreviewTest,
	"GatherAndHunt.GatherAndHunt.Planner.PreviewMatchesSubmit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntTurnPlannerPreviewTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* Session = nullptr;
	UGatherAndHuntTurnPlanner* const Planner = MakePlanner(Session);

	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Forward);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Forward);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::YawLeft);

	const TArray<FGatherAndHuntPlannerTurtleTrack> Tracks = Planner->GetPreviewTracks();
	TestEqual(TEXT("One track for the one turtle"), Tracks.Num(), 1);
	TestEqual(TEXT("Start + three ticks"), Tracks[0].Steps.Num(), 4);
	TestEqual(TEXT("Preview ends two cells east"), Tracks[0].Steps.Last().GridPosition, FIntVector(2, 0, 0));
	TestEqual(TEXT("Preview ends facing north"), Tracks[0].Steps.Last().Heading, 0);
	TestTrue(TEXT("Second step claims the objective"), Tracks[0].Steps[2].bClaimedObjective);
	TestEqual(TEXT("Preview counts the claim"), Planner->GetPreviewSummary().ObjectivesClaimed, 1);

	FGatherAndHuntPuzzlePlanValidation Result;
	TestTrue(TEXT("Valid plan submits"), Planner->Submit(Result));
	const FGatherAndHuntPuzzleBoardSnapshot Board = Session->GetCurrentBoardSnapshot();
	TestEqual(TEXT("Session matches the preview position"), Board.Turtles[0].GridPosition, FIntVector(2, 0, 0));
	TestEqual(TEXT("Session matches the preview heading"), Board.Turtles[0].Heading, 0);
	TestTrue(TEXT("Session claimed the objective"), Board.Objectives[0].bClaimed);
	TestEqual(TEXT("Plan clears after submit"), Planner->GetTotalCost(), 0);
	TestEqual(TEXT("Planner picked up the new turn"), Planner->GetTurtleSummaries()[0].GridPosition, FIntVector(2, 0, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntTurnPlannerHazardPreviewTest,
	"GatherAndHunt.GatherAndHunt.Planner.PreviewShowsHazardKill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntTurnPlannerHazardPreviewTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* Session = nullptr;
	UGatherAndHuntTurnPlanner* const Planner = MakePlanner(Session, true);

	// The hazard patrols (0,2,0) -> (0,1,0) -> (0,0,0); waiting in place gets hit on turn 2.
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
	FGatherAndHuntPuzzlePlanValidation Result;
	TestTrue(TEXT("First turn submits"), Planner->Submit(Result));
	TestTrue(TEXT("Turtle survives turn one"), Session->GetCurrentBoardSnapshot().Turtles[0].bAlive);

	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
	TestEqual(TEXT("Preview predicts the turtle is lost"), Planner->GetPreviewSummary().TurtlesLost, 1);
	TestEqual(TEXT("Turtle fate is destroyed"), Planner->GetTurtleSummaries()[0].PreviewFate, EGatherAndHuntPlannerStepKind::Destroyed);

	Planner->ClearAll();
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Forward);
	TestEqual(TEXT("Stepping away avoids the hazard"), Planner->GetPreviewSummary().TurtlesLost, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntTurnPlannerOpponentAuthoringTest,
	"GatherAndHunt.GatherAndHunt.Planner.OpponentAuthoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntTurnPlannerOpponentAuthoringTest::RunTest(const FString&)
{
	// 5x5x5 board: player turtle 1 at (0,0,0) facing East, opponent turtle 2 at
	// (4,0,0) facing West, no scripted turns yet.
	ULevelSave* const Puzzle = NewObject<ULevelSave>(GetTransientPackage());
	Puzzle->LevelName = TEXT("OpponentAuthoring");
	Puzzle->GridSize = FVector(400.0f);
	Puzzle->GridCellSize = 100.0f;
	Puzzle->TargetScore = 100;
	Puzzle->PieceSaveDataArray.Add(MakePlannerPiece(1, PieceTypeEnum::BP_PlayerOwnedPiece, FVector::ZeroVector));
	FPieceSaveData Opponent = MakePlannerPiece(2, PieceTypeEnum::BP_PlayerOwnedPiece, FVector(400.0f, 0.0f, 0.0f));
	Opponent.PieceOwnerID = 1;
	Opponent.Rotation = FRotator(0.0f, 180.0f, 0.0f);
	Puzzle->PieceSaveDataArray.Add(Opponent);

	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	TestTrue(TEXT("Puzzle loads"), Session->LoadPuzzleFromLevelSave(Puzzle));
	UGatherAndHuntTurnPlanner* const Planner = NewObject<UGatherAndHuntTurnPlanner>();
	Planner->BindToSession(Session);

	TestEqual(TEXT("Player side lists the player turtle"), Planner->GetSelectedTurtleId(), 1);
	Planner->SetEditingOpponent(true);
	TestEqual(TEXT("Opponent side lists the opponent turtle"), Planner->GetSelectedTurtleId(), 2);
	TestFalse(TEXT("Cannot submit while editing the opponent"), Planner->CanSubmit());
	TestEqual(TEXT("No scripted turn yet, so the draft is empty"), Planner->GetTurtleMoves(2).Num(), 0);

	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Forward);
	TestTrue(TEXT("Editing marks the draft unsaved"), Planner->HasUnsavedOpponentDraft());

	FGatherAndHuntPuzzlePlanValidation Result;
	TestTrue(TEXT("Draft saves as the turn-0 script"), Planner->SaveOpponentTurn(Result));
	TestFalse(TEXT("Saved draft is clean"), Planner->HasUnsavedOpponentDraft());
	TestTrue(TEXT("Session has a turn-0 script"), Session->HasScriptedOpponentTurn(0));
	TestTrue(TEXT("Session has unsaved file changes"), Session->HasUnsavedChanges());
	const FGatherAndHuntPuzzleTurnPlan Scripted = Session->GetScriptedOpponentTurn(0);
	TestTrue(TEXT("Script holds the forward move for turtle 2"),
		Scripted.TurtlePlans.Num() == 1 && Scripted.TurtlePlans[0].TurtleId == 2
		&& Scripted.TurtlePlans[0].Moves == TArray<EGatherAndHuntPuzzleMoveKind>{EGatherAndHuntPuzzleMoveKind::Forward});

	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
	Planner->RevertOpponentDraft();
	TestEqual(TEXT("Revert restores the saved script"), Planner->GetTurtleMoves(2).Num(), 1);
	TestFalse(TEXT("Reverted draft is clean"), Planner->HasUnsavedOpponentDraft());

	// Back on the player side the preview runs against the new script.
	Planner->SetEditingOpponent(false);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
	for (const FGatherAndHuntPlannerTurtleTrack& Track : Planner->GetPreviewTracks())
	{
		if (Track.TurtleId == 2)
		{
			TestEqual(TEXT("Preview moves the opponent per its script"), Track.Steps.Last().GridPosition, FIntVector(3, 0, 0));
		}
	}

	TestTrue(TEXT("Player turn submits"), Planner->Submit(Result));
	const FGatherAndHuntPuzzleBoardSnapshot Board = Session->GetCurrentBoardSnapshot();
	for (const FGatherAndHuntPuzzleTurtleState& Turtle : Board.Turtles)
	{
		if (Turtle.TurtleId == 2)
		{
			TestEqual(TEXT("Session resolved the scripted move"), Turtle.GridPosition, FIntVector(3, 0, 0));
		}
	}

	Planner->SetEditingOpponent(true);
	TestEqual(TEXT("Turn 1 has no script, so its draft starts empty"), Planner->GetTurtleMoves(2).Num(), 0);

	// Round-trip the script through a board file.
	const FString RelativeFile = TEXT("Automation/PlannerOpponentAuthoring.dat");
	TestTrue(TEXT("Puzzle saves to disk"), Session->SavePuzzleToProjectFile(RelativeFile));
	TestFalse(TEXT("Saving clears the unsaved flag"), Session->HasUnsavedChanges());
	const ULevelSave* const Reloaded = UGatherAndHuntLevelEditorLibrary::LoadLevelSaveFromProjectFile(GetTransientPackage(), RelativeFile);
	TestTrue(TEXT("Reloaded file keeps the turn-0 script"),
		Reloaded != nullptr && Reloaded->ScriptedOpponentTurns.Num() == 1 && Reloaded->ScriptedOpponentTurns[0].TurnNumber == 0);
	IFileManager::Get().Delete(*FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("BoardSetups") / RelativeFile));
	return true;
}

#endif
