#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GatherAndHuntBoardEditor.h"
#include "GatherAndHuntFrontEnd.h"
#include "GatherAndHuntPuzzleSession.h"
#include "ULevelSave.h"

namespace
{
	// Empty 5x4x2 board bound to a fresh session.
	UGatherAndHuntBoardEditor* MakeEditor(UGatherAndHuntPuzzleSession*& OutSession)
	{
		OutSession = NewObject<UGatherAndHuntPuzzleSession>();
		UGatherAndHuntBoardEditor* const Editor = NewObject<UGatherAndHuntBoardEditor>();
		Editor->BindToSession(OutSession);
		Editor->NewPuzzle(TEXT("EditorTest"), FIntVector(5, 4, 2));
		return Editor;
	}

	int32 CountIssues(const UGatherAndHuntBoardEditor* Editor, const bool bErrors)
	{
		int32 Count = 0;
		for (const FLevelEditorValidationIssue& Issue : Editor->GetIssues())
		{
			Count += Issue.bIsError == bErrors ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntBoardEditorPlacementTest,
	"GatherAndHunt.GatherAndHunt.BoardEditor.Placement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntBoardEditorPlacementTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* Session = nullptr;
	UGatherAndHuntBoardEditor* const Editor = MakeEditor(Session);

	TestTrue(TEXT("New puzzle loads into the session"), Session->IsPuzzleLoaded());
	TestEqual(TEXT("Board has the requested size"), Editor->GetCellCount(), FIntVector(5, 4, 2));
	TestEqual(TEXT("Session board matches"), Session->GetCurrentBoardSnapshot().GridSize, FIntVector(5, 4, 2));
	TestTrue(TEXT("An empty board reports it has no player turtle"), CountIssues(Editor, true) > 0);

	Editor->SetCursor(FIntVector(1, 2, 0));
	TestTrue(TEXT("Placing a turtle succeeds"), Editor->PlacePiece(EGatherAndHuntBoardPieceKind::Turtle));
	TestFalse(TEXT("Placing into an occupied cell fails"), Editor->PlacePiece(EGatherAndHuntBoardPieceKind::Block));

	FGatherAndHuntBoardEditorPiece Piece;
	TestTrue(TEXT("The new piece is selected"), Editor->GetSelectedPiece(Piece));
	TestEqual(TEXT("It is a turtle"), Piece.Kind, EGatherAndHuntBoardPieceKind::Turtle);
	TestEqual(TEXT("It defaults to the player"), Piece.Role, EGatherAndHuntTurtleRole::Player);
	TestEqual(TEXT("Session sees one turtle"), Session->GetCurrentBoardSnapshot().Turtles.Num(), 1);
	TestEqual(TEXT("Session turtle sits in the cursor cell"), Session->GetCurrentBoardSnapshot().Turtles[0].GridPosition, FIntVector(1, 2, 0));
	TestEqual(TEXT("A player turtle clears the errors"), CountIssues(Editor, true), 0);

	Editor->SetCursor(FIntVector(3, 0, 1));
	Editor->PlacePiece(EGatherAndHuntBoardPieceKind::GoldObjective);
	TestEqual(TEXT("Gold objective is worth 5"), Session->GetCurrentBoardSnapshot().Objectives[0].PointValue, 5);

	Editor->SetCursor(FIntVector(0, 0, 0));
	Editor->PlacePiece(EGatherAndHuntBoardPieceKind::Hunter);
	TestEqual(TEXT("Hunter becomes a hunter obstacle"), Session->GetCurrentBoardSnapshot().Obstacles[0].ObstacleKind, 2);

	// Cursor clamps to the board.
	Editor->SetCursor(FIntVector(99, -5, 7));
	TestEqual(TEXT("Cursor clamps"), Editor->GetCursor(), FIntVector(4, 0, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntBoardEditorEditingTest,
	"GatherAndHunt.GatherAndHunt.BoardEditor.MoveRotateRole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntBoardEditorEditingTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* Session = nullptr;
	UGatherAndHuntBoardEditor* const Editor = MakeEditor(Session);
	Editor->SetCursor(FIntVector(1, 1, 0));
	Editor->PlacePiece(EGatherAndHuntBoardPieceKind::Turtle);
	Editor->SetCursor(FIntVector(2, 1, 0));
	Editor->PlacePiece(EGatherAndHuntBoardPieceKind::Block);
	Editor->SetCursor(FIntVector(1, 1, 0));

	TestFalse(TEXT("Can't move into an occupied cell"), Editor->MoveSelectedPiece(FIntVector(1, 0, 0)));
	TestFalse(TEXT("Can't move off the board"), Editor->MoveSelectedPiece(FIntVector(0, 0, -1)));
	TestTrue(TEXT("Moves into a free cell"), Editor->MoveSelectedPiece(FIntVector(0, 1, 1)));
	TestEqual(TEXT("Cursor follows the piece"), Editor->GetCursor(), FIntVector(1, 2, 1));

	FGatherAndHuntBoardEditorPiece Piece;
	Editor->GetSelectedPiece(Piece);
	TestEqual(TEXT("Turtles start facing East"), Piece.Heading, 1);
	Editor->RotateSelectedPiece(1);
	Editor->GetSelectedPiece(Piece);
	TestEqual(TEXT("Rotating right faces South"), Piece.Heading, 2);
	TestEqual(TEXT("Session agrees on the heading"), Session->GetCurrentBoardSnapshot().Turtles[0].Heading, 2);

	// Pitching up keeps the heading (a vertical facing reads it from the yaw).
	TestTrue(TEXT("Pitch up succeeds"), Editor->PitchSelectedPiece(1));
	Editor->GetSelectedPiece(Piece);
	TestEqual(TEXT("Pitched up"), Piece.Pitch, 2);
	TestEqual(TEXT("Heading survives pitching up"), Piece.Heading, 2);
	TestFalse(TEXT("Can't pitch past straight up"), Editor->PitchSelectedPiece(1));
	TestEqual(TEXT("Session sees the pitch"), Session->GetCurrentBoardSnapshot().Turtles[0].Pitch, 2);
	TestEqual(TEXT("Session sees the heading"), Session->GetCurrentBoardSnapshot().Turtles[0].Heading, 2);

	TestTrue(TEXT("Cycle to ally"), Editor->CycleSelectedTurtleRole());
	Editor->GetSelectedPiece(Piece);
	TestEqual(TEXT("Now an ally"), Piece.Role, EGatherAndHuntTurtleRole::Ally);
	TestEqual(TEXT("Session sees the ally"), Session->GetTurtleRole(Piece.PieceId), EGatherAndHuntTurtleRole::Ally);
	Editor->CycleSelectedTurtleRole();
	TestEqual(TEXT("Then an opponent"), Session->GetTurtleRole(Piece.PieceId), EGatherAndHuntTurtleRole::Opponent);

	// Blocks have no facing.
	Editor->SetCursor(FIntVector(2, 1, 0));
	TestFalse(TEXT("Blocks don't rotate"), Editor->RotateSelectedPiece(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntBoardEditorHazardTest,
	"GatherAndHunt.GatherAndHunt.BoardEditor.Hazards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntBoardEditorHazardTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* Session = nullptr;
	UGatherAndHuntBoardEditor* const Editor = MakeEditor(Session);
	Editor->SetCursor(FIntVector(0, 0, 0));
	Editor->PlacePiece(EGatherAndHuntBoardPieceKind::PatrolHazard);

	FGatherAndHuntBoardEditorPiece Piece;
	Editor->GetSelectedPiece(Piece);
	// Length 2 facing East: (0,0) (1,0) (2,0) (1,0) and back to the start.
	TArray<FIntVector> Path = Editor->GetHazardPath(Piece.PieceId);
	TestEqual(TEXT("Patrol cycle plus the return"), Path.Num(), 5);
	TestEqual(TEXT("Patrol reaches two cells out"), Path[2], FIntVector(2, 0, 0));
	TestEqual(TEXT("Session sees a wandering obstacle"), Session->GetCurrentBoardSnapshot().Obstacles[0].ObstacleKind, 1);

	TestTrue(TEXT("Stride grows"), Editor->AdjustSelectedHazard(EGatherAndHuntHazardParam::Stride, 1));
	Path = Editor->GetHazardPath(Piece.PieceId);
	TestEqual(TEXT("Stride 2 reaches four cells out"), Path[2], FIntVector(4, 0, 0));
	TestFalse(TEXT("Width means nothing to a patrol"), Editor->AdjustSelectedHazard(EGatherAndHuntHazardParam::Width, 1));

	TestTrue(TEXT("Length grows"), Editor->AdjustSelectedHazard(EGatherAndHuntHazardParam::Length, 1));
	TestTrue(TEXT("Leaving the board is a warning"), CountIssues(Editor, false) > 0);

	// Phase wraps around the cycle.
	Editor->AdjustSelectedHazard(EGatherAndHuntHazardParam::Phase, -1);
	Editor->GetSelectedPiece(Piece);
	TestEqual(TEXT("Phase wraps to the end of the cycle"), Piece.Phase, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntBoardEditorUndoTest,
	"GatherAndHunt.GatherAndHunt.BoardEditor.UndoAndScripts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntBoardEditorUndoTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* Session = nullptr;
	UGatherAndHuntBoardEditor* const Editor = MakeEditor(Session);
	Editor->PlacePiece(EGatherAndHuntBoardPieceKind::Turtle);
	Editor->SetCursor(FIntVector(4, 3, 0));
	Editor->PlacePiece(EGatherAndHuntBoardPieceKind::Turtle);
	Editor->SetSelectedTurtleRole(EGatherAndHuntTurtleRole::Opponent);
	FGatherAndHuntBoardEditorPiece Opponent;
	Editor->GetSelectedPiece(Opponent);

	// Script the opponent, then pull the edit back into the editor.
	FGatherAndHuntPuzzleTurnPlan Script;
	FGatherAndHuntPuzzleTurtlePlan& TurtlePlan = Script.TurtlePlans.AddDefaulted_GetRef();
	TurtlePlan.TurtleId = Opponent.PieceId;
	TurtlePlan.Moves.Add(EGatherAndHuntPuzzleMoveKind::Wait);
	Session->SetScriptedTurn(0, Script);
	Editor->SyncFromSession();
	TestFalse(TEXT("Syncing clears the undo history"), Editor->CanUndo());

	Editor->SetCursor(FIntVector(4, 3, 0));
	TestTrue(TEXT("Removing the opponent succeeds"), Editor->RemoveSelectedPiece());
	TestFalse(TEXT("Its scripted moves go with it"), Session->HasScriptedTurn(0));
	TestEqual(TEXT("One turtle left"), Session->GetCurrentBoardSnapshot().Turtles.Num(), 1);

	TestTrue(TEXT("Undo is available"), Editor->CanUndo());
	Editor->Undo();
	TestEqual(TEXT("Undo restores the turtle"), Session->GetCurrentBoardSnapshot().Turtles.Num(), 2);
	TestTrue(TEXT("Undo restores its script"), Session->HasScriptedTurn(0));
	TestTrue(TEXT("Redo is available"), Editor->CanRedo());
	Editor->Redo();
	TestEqual(TEXT("Redo removes it again"), Session->GetCurrentBoardSnapshot().Turtles.Num(), 1);

	// Metadata edits are undoable and skip no-op changes.
	Editor->SetTargetScore(3);
	Editor->SetTargetScore(3);
	TestEqual(TEXT("Target score set"), Session->GetTargetScore(), 3);
	Editor->Undo();
	TestEqual(TEXT("One undo reverts the single real change"), Session->GetTargetScore(), 0);

	// Shrinking refuses to cut off pieces.
	Editor->SetCursor(FIntVector(4, 0, 0));
	Editor->PlacePiece(EGatherAndHuntBoardPieceKind::Block);
	TestFalse(TEXT("Can't shrink past a piece"), Editor->ResizeBoard(FIntVector(4, 4, 2)));
	TestTrue(TEXT("Can grow"), Editor->ResizeBoard(FIntVector(7, 4, 2)));
	TestEqual(TEXT("Session board grows"), Session->GetCurrentBoardSnapshot().GridSize, FIntVector(7, 4, 2));
	TestTrue(TEXT("Edits mark the puzzle unsaved"), Session->HasUnsavedChanges());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntBoardEditorSaveTest,
	"GatherAndHunt.GatherAndHunt.BoardEditor.SaveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntBoardEditorSaveTest::RunTest(const FString&)
{
	UGatherAndHuntPuzzleSession* Session = nullptr;
	UGatherAndHuntBoardEditor* const Editor = MakeEditor(Session);
	Editor->SetCursor(FIntVector(2, 2, 1));
	Editor->PlacePiece(EGatherAndHuntBoardPieceKind::Turtle);
	Editor->RotateSelectedPiece(-1);
	Editor->PitchSelectedPiece(-1);
	Editor->SetPuzzleName(TEXT("  Saved from the editor  "));
	Editor->SetIntroText(TEXT("Hello."));

	const FString File = UGatherAndHuntFrontEnd::NormalizePuzzleFilename(TEXT("Automation/Board Editor RoundTrip"));
	TestEqual(TEXT("Filenames lose spaces and gain .dat"), File, FString(TEXT("Automation/Board_Editor_RoundTrip.dat")));
	TestTrue(TEXT("Save succeeds"), Session->SavePuzzleToProjectFile(File));
	TestFalse(TEXT("Saving clears the unsaved flag"), Session->HasUnsavedChanges());

	UGatherAndHuntPuzzleSession* const Reloaded = NewObject<UGatherAndHuntPuzzleSession>();
	TestTrue(TEXT("The file loads"), Reloaded->LoadPuzzleFromRelativeFilename(File));
	TestEqual(TEXT("Name is trimmed and kept"), Reloaded->GetLoadedPuzzleName(), FString(TEXT("Saved from the editor")));
	TestEqual(TEXT("Intro kept"), Reloaded->GetIntroText(), FString(TEXT("Hello.")));
	const FGatherAndHuntPuzzleBoardSnapshot Board = Reloaded->GetCurrentBoardSnapshot();
	TestEqual(TEXT("Size kept"), Board.GridSize, FIntVector(5, 4, 2));
	TestEqual(TEXT("Turtle kept"), Board.Turtles.Num(), 1);
	TestEqual(TEXT("Cell kept"), Board.Turtles[0].GridPosition, FIntVector(2, 2, 1));
	TestEqual(TEXT("Heading kept (North after a left turn from East)"), Board.Turtles[0].Heading, 0);
	TestEqual(TEXT("Pitch kept"), Board.Turtles[0].Pitch, 0);
	TestTrue(TEXT("The file shows up in the listing with automation files"),
		UGatherAndHuntFrontEnd::ListPuzzles(true).ContainsByPredicate([&File](const FGatherAndHuntPuzzleListing& Listing) { return Listing.RelativeFilename == File; }));
	TestFalse(TEXT("Automation files stay out of the normal listing"),
		UGatherAndHuntFrontEnd::ListPuzzles(false).ContainsByPredicate([&File](const FGatherAndHuntPuzzleListing& Listing) { return Listing.RelativeFilename == File; }));
	return true;
}

#endif
