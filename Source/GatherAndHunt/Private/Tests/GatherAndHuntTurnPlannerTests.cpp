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

	// Drag-to-reorder: move #1 to the end and back.
	Planner->SelectMove(INDEX_NONE);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::YawLeft); // Now 8 moves, YawLeft last.
	Planner->MoveMove(7, 0);
	TestEqual(TEXT("MoveMove brings the last move to the front"), Planner->GetTurtleMoves(1)[0], EGatherAndHuntPuzzleMoveKind::YawLeft);
	TestEqual(TEXT("Moved move becomes selected"), Planner->GetSelectedMoveIndex(), 0);
	Planner->MoveMove(0, 5);
	TestEqual(TEXT("MoveMove drops mid-queue"), Planner->GetTurtleMoves(1)[5], EGatherAndHuntPuzzleMoveKind::YawLeft);
	TestEqual(TEXT("Reorder keeps the move count"), Planner->GetTurtleMoves(1).Num(), 8);
	Planner->MoveMove(0, 99);
	TestEqual(TEXT("Out-of-range target is ignored"), Planner->GetTurtleMoves(1)[5], EGatherAndHuntPuzzleMoveKind::YawLeft);

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

namespace
{
	// 5x5x5 board with one turtle per role:
	//   T1 player   at (0,0,0) facing East
	//   T2 opponent at (4,0,0) facing West
	//   T3 ally     at (0,4,0) facing East (player team, script-controlled)
	ULevelSave* MakeRolesPuzzle()
	{
		ULevelSave* const Puzzle = NewObject<ULevelSave>(GetTransientPackage());
		Puzzle->LevelName = TEXT("Roles");
		Puzzle->GridSize = FVector(400.0f);
		Puzzle->GridCellSize = 100.0f;
		Puzzle->TargetScore = 100;
		Puzzle->PieceSaveDataArray.Add(MakePlannerPiece(1, PieceTypeEnum::BP_PlayerOwnedPiece, FVector::ZeroVector));

		FPieceSaveData Opponent = MakePlannerPiece(2, PieceTypeEnum::BP_PlayerOwnedPiece, FVector(400.0f, 0.0f, 0.0f));
		UGatherAndHuntLevelEditorLibrary::SetTurtleRole(Opponent, EGatherAndHuntTurtleRole::Opponent);
		Opponent.Rotation = FRotator(0.0f, 180.0f, 0.0f);
		Puzzle->PieceSaveDataArray.Add(Opponent);

		FPieceSaveData Ally = MakePlannerPiece(3, PieceTypeEnum::BP_PlayerOwnedPiece, FVector(0.0f, 400.0f, 0.0f));
		UGatherAndHuntLevelEditorLibrary::SetTurtleRole(Ally, EGatherAndHuntTurtleRole::Ally);
		Puzzle->PieceSaveDataArray.Add(Ally);
		return Puzzle;
	}

	FIntVector FindTurtlePosition(const FGatherAndHuntPuzzleBoardSnapshot& Board, const int32 TurtleId)
	{
		for (const FGatherAndHuntPuzzleTurtleState& Turtle : Board.Turtles)
		{
			if (Turtle.TurtleId == TurtleId)
			{
				return Turtle.GridPosition;
			}
		}
		return FIntVector(-1);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntTurnPlannerRolesTest,
	"GatherAndHunt.GatherAndHunt.Planner.Roles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntTurnPlannerRolesTest::RunTest(const FString&)
{
	ULevelSave* const Puzzle = MakeRolesPuzzle();
	TestEqual(TEXT("Editor role: player"), UGatherAndHuntLevelEditorLibrary::GetTurtleRole(Puzzle->PieceSaveDataArray[0]), EGatherAndHuntTurtleRole::Player);
	TestEqual(TEXT("Editor role: opponent"), UGatherAndHuntLevelEditorLibrary::GetTurtleRole(Puzzle->PieceSaveDataArray[1]), EGatherAndHuntTurtleRole::Opponent);
	TestEqual(TEXT("Editor role: ally"), UGatherAndHuntLevelEditorLibrary::GetTurtleRole(Puzzle->PieceSaveDataArray[2]), EGatherAndHuntTurtleRole::Ally);
	TestEqual(TEXT("Ally stays on the player team"), Puzzle->PieceSaveDataArray[2].PieceOwnerID, 0);

	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	TestTrue(TEXT("Puzzle loads"), Session->LoadPuzzleFromLevelSave(Puzzle));
	TestEqual(TEXT("Session role: player"), Session->GetTurtleRole(1), EGatherAndHuntTurtleRole::Player);
	TestEqual(TEXT("Session role: opponent"), Session->GetTurtleRole(2), EGatherAndHuntTurtleRole::Opponent);
	TestEqual(TEXT("Session role: ally"), Session->GetTurtleRole(3), EGatherAndHuntTurtleRole::Ally);
	for (const FGatherAndHuntPuzzleTurtleState& Turtle : Session->GetCurrentBoardSnapshot().Turtles)
	{
		TestEqual(*FString::Printf(TEXT("Snapshot carries the role of T%d"), Turtle.TurtleId), Turtle.Role, Session->GetTurtleRole(Turtle.TurtleId));
	}

	UGatherAndHuntTurnPlanner* const Planner = NewObject<UGatherAndHuntTurnPlanner>();
	Planner->BindToSession(Session);
	TestEqual(TEXT("Player mode lists only the player turtle"), Planner->GetTurtleSummaries().Num(), 1);
	Planner->SelectTurtle(3);
	TestEqual(TEXT("Ally cannot be selected in player mode"), Planner->GetSelectedTurtleId(), 1);

	Planner->SetEditingScript(true);
	const TArray<FGatherAndHuntPlannerTurtleSummary> Scripted = Planner->GetTurtleSummaries();
	TestTrue(TEXT("Script mode lists opponent and ally"),
		Scripted.Num() == 2 && Scripted[0].Role == EGatherAndHuntTurtleRole::Opponent && Scripted[1].Role == EGatherAndHuntTurtleRole::Ally);

	// Each scripted team has its own budget: 12 moves each (cost 20) is fine.
	for (const int32 TurtleId : {2, 3})
	{
		Planner->SelectTurtle(TurtleId);
		for (int32 Index = 0; Index < 12; ++Index)
		{
			Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
		}
	}
	TestTrue(TEXT("Full budget on both scripted teams is valid"), Planner->GetValidation().bIsValid);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
	TestFalse(TEXT("Over budget on the ally team is invalid"), Planner->GetValidation().bIsValid);
	TestEqual(TEXT("Budget shown is the ally team"), Planner->GetTotalCost(), 23);
	Planner->SelectTurtle(2);
	TestEqual(TEXT("Selecting the opponent shows its own budget"), Planner->GetTotalCost(), 20);
	Planner->SelectTurtle(3);
	Planner->RemoveSelectedMove();
	TestTrue(TEXT("Trimming the ally back under budget is valid"), Planner->GetValidation().bIsValid);

	// The player cannot plan an ally directly.
	FGatherAndHuntPuzzleTurnPlan AllyPlan;
	FGatherAndHuntPuzzleTurtlePlan& TurtlePlan = AllyPlan.TurtlePlans.AddDefaulted_GetRef();
	TurtlePlan.TurtleId = 3;
	TurtlePlan.Moves.Add(EGatherAndHuntPuzzleMoveKind::Forward);
	FGatherAndHuntPuzzlePlanValidation Result;
	TestFalse(TEXT("Submitting an ally move is rejected"), Session->SubmitSoloTurnPlan(AllyPlan, Result));
	TestEqual(TEXT("Rejection names the ally"), Result.OffendingTurtleId, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntTurnPlannerScriptAuthoringTest,
	"GatherAndHunt.GatherAndHunt.Planner.ScriptAuthoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntTurnPlannerScriptAuthoringTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	TestTrue(TEXT("Puzzle loads"), Session->LoadPuzzleFromLevelSave(MakeRolesPuzzle()));
	UGatherAndHuntTurnPlanner* const Planner = NewObject<UGatherAndHuntTurnPlanner>();
	Planner->BindToSession(Session);

	Planner->SetEditingScript(true);
	TestFalse(TEXT("Cannot submit while editing the script"), Planner->CanSubmit());
	TestEqual(TEXT("No scripted turn yet, so the draft is empty"), Planner->GetTurtleMoves(2).Num(), 0);

	Planner->SelectTurtle(2);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Forward);
	Planner->SelectTurtle(3);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Forward);
	TestTrue(TEXT("Editing marks the draft unsaved"), Planner->HasUnsavedScriptDraft());

	FGatherAndHuntPuzzlePlanValidation Result;
	TestTrue(TEXT("Draft saves as the turn-0 script"), Planner->SaveScriptedTurn(Result));
	TestFalse(TEXT("Saved draft is clean"), Planner->HasUnsavedScriptDraft());
	TestTrue(TEXT("Session has a turn-0 script"), Session->HasScriptedTurn(0));
	TestTrue(TEXT("Session has unsaved file changes"), Session->HasUnsavedChanges());
	TestEqual(TEXT("Script holds the ally and the opponent"), Session->GetScriptedTurn(0).TurtlePlans.Num(), 2);

	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
	Planner->RevertScriptDraft();
	TestEqual(TEXT("Revert restores the saved script"), Planner->GetTurtleMoves(3).Num(), 1);
	TestFalse(TEXT("Reverted draft is clean"), Planner->HasUnsavedScriptDraft());

	// Back on the player side the preview runs the script for both teams.
	Planner->SetEditingScript(false);
	Planner->AddMove(EGatherAndHuntPuzzleMoveKind::Wait);
	for (const FGatherAndHuntPlannerTurtleTrack& Track : Planner->GetPreviewTracks())
	{
		if (Track.TurtleId == 2)
		{
			TestEqual(TEXT("Preview moves the opponent per the script"), Track.Steps.Last().GridPosition, FIntVector(3, 0, 0));
		}
		if (Track.TurtleId == 3)
		{
			TestEqual(TEXT("Preview moves the ally per the script"), Track.Steps.Last().GridPosition, FIntVector(1, 4, 0));
			TestEqual(TEXT("Track carries the ally role"), Track.Role, EGatherAndHuntTurtleRole::Ally);
		}
	}

	TestTrue(TEXT("Player turn submits"), Planner->Submit(Result));
	const FGatherAndHuntPuzzleBoardSnapshot Board = Session->GetCurrentBoardSnapshot();
	TestEqual(TEXT("Session resolved the scripted opponent move"), FindTurtlePosition(Board, 2), FIntVector(3, 0, 0));
	TestEqual(TEXT("Session resolved the scripted ally move"), FindTurtlePosition(Board, 3), FIntVector(1, 4, 0));

	Planner->SetEditingScript(true);
	TestEqual(TEXT("Turn 1 has no script, so its draft starts empty"), Planner->GetTurtleMoves(3).Num(), 0);

	// Round-trip the script and the ally flag through a board file.
	const FString RelativeFile = TEXT("Automation/PlannerScriptAuthoring.dat");
	TestTrue(TEXT("Puzzle saves to disk"), Session->SavePuzzleToProjectFile(RelativeFile));
	TestFalse(TEXT("Saving clears the unsaved flag"), Session->HasUnsavedChanges());
	ULevelSave* const Reloaded = UGatherAndHuntLevelEditorLibrary::LoadLevelSaveFromProjectFile(GetTransientPackage(), RelativeFile);
	TestTrue(TEXT("Reloaded file keeps the turn-0 script"),
		Reloaded != nullptr && Reloaded->ScriptedOpponentTurns.Num() == 1 && Reloaded->ScriptedOpponentTurns[0].TurnNumber == 0);
	if (Reloaded != nullptr)
	{
		UGatherAndHuntPuzzleSession* const ReloadedSession = NewObject<UGatherAndHuntPuzzleSession>();
		TestTrue(TEXT("Reloaded puzzle loads"), ReloadedSession->LoadPuzzleFromLevelSave(Reloaded));
		TestEqual(TEXT("Reloaded file keeps the ally role"), ReloadedSession->GetTurtleRole(3), EGatherAndHuntTurtleRole::Ally);
	}
	IFileManager::Get().Delete(*FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("BoardSetups") / RelativeFile));
	return true;
}


#endif
