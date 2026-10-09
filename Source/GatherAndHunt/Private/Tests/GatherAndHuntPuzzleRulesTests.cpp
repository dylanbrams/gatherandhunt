#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GatherAndHuntLevelEditorLibrary.h"
#include "GatherAndHuntPuzzleSession.h"
#include "GatherAndHuntTurnPlanner.h"

namespace
{
	FPieceSaveData MakeRulesPiece(const int32 Id, const PieceTypeEnum Type, const FIntVector& Cell)
	{
		FPieceSaveData Data;
		Data.PieceSaveID = Id;
		Data.PieceType = Type;
		Data.Position = FVector(Cell) * 100.0f;
		return Data; // Rotation zero: faces East (+X).
	}

	// 5x3x1: turtles 1 at (0,0) and 2 at (0,2), a ball at (2,0).
	ULevelSave* MakeRulesPuzzle()
	{
		ULevelSave* const Puzzle = UGatherAndHuntLevelEditorLibrary::CreateEmptyLevelSave(GetTransientPackage(), TEXT("Rules"), 0, FVector(400.0f, 200.0f, 1.0f));
		Puzzle->PieceSaveDataArray.Add(MakeRulesPiece(1, PieceTypeEnum::BP_PlayerOwnedPiece, FIntVector(0, 0, 0)));
		Puzzle->PieceSaveDataArray.Add(MakeRulesPiece(2, PieceTypeEnum::BP_PlayerOwnedPiece, FIntVector(0, 2, 0)));
		Puzzle->PieceSaveDataArray.Add(MakeRulesPiece(3, PieceTypeEnum::BP_Objective, FIntVector(2, 0, 0)));
		return Puzzle;
	}

	FGatherAndHuntPuzzleTurnPlan MakePlan(const TMap<int32, TArray<EGatherAndHuntPuzzleMoveKind>>& Moves)
	{
		FGatherAndHuntPuzzleTurnPlan Plan;
		for (const TPair<int32, TArray<EGatherAndHuntPuzzleMoveKind>>& Entry : Moves)
		{
			FGatherAndHuntPuzzleTurtlePlan& TurtlePlan = Plan.TurtlePlans.AddDefaulted_GetRef();
			TurtlePlan.TurtleId = Entry.Key;
			TurtlePlan.Moves = Entry.Value;
		}
		return Plan;
	}

	TArray<EGatherAndHuntPuzzleMoveKind> ParseMoves(const TCHAR* Letters)
	{
		TArray<EGatherAndHuntPuzzleMoveKind> Moves;
		for (const TCHAR* Letter = Letters; *Letter != 0; ++Letter)
		{
			switch (*Letter)
			{
			case 'F': Moves.Add(EGatherAndHuntPuzzleMoveKind::Forward); break;
			case 'L': Moves.Add(EGatherAndHuntPuzzleMoveKind::YawLeft); break;
			case 'R': Moves.Add(EGatherAndHuntPuzzleMoveKind::YawRight); break;
			case 'U': Moves.Add(EGatherAndHuntPuzzleMoveKind::PitchUp); break;
			case 'D': Moves.Add(EGatherAndHuntPuzzleMoveKind::PitchDown); break;
			default: Moves.Add(EGatherAndHuntPuzzleMoveKind::Wait); break;
			}
		}
		return Moves;
	}

	constexpr EGatherAndHuntPuzzleMoveKind F = EGatherAndHuntPuzzleMoveKind::Forward;
	constexpr EGatherAndHuntPuzzleMoveKind L = EGatherAndHuntPuzzleMoveKind::YawLeft;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntPuzzleRulesTest,
	"GatherAndHunt.GatherAndHunt.PuzzleRules.MovesShipsAndGoal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntPuzzleRulesTest::RunTest(const FString&)
{
	// Allowed moves: forward only.
	{
		ULevelSave* const Puzzle = MakeRulesPuzzle();
		Puzzle->AllowedMoveMask = 1 << static_cast<int32>(EGatherAndHuntPuzzleMoveKind::Forward);
		UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
		Session->LoadPuzzleFromLevelSave(Puzzle);

		FGatherAndHuntPuzzlePlanValidation Validation;
		TestFalse(TEXT("A turn is refused when the puzzle doesn't offer it"), Session->SubmitSoloTurnPlan(MakePlan({{1, {F, L}}}), Validation));
		TestEqual(TEXT("The turn is the offending move"), Validation.OffendingMoveIndex, 1);

		UGatherAndHuntTurnPlanner* const Planner = NewObject<UGatherAndHuntTurnPlanner>();
		Planner->BindToSession(Session);
		Planner->AddMove(L);
		Planner->AddMove(F);
		TestEqual(TEXT("The planner ignores moves the puzzle doesn't offer"), Planner->GetTurtleMoves(Planner->GetSelectedTurtleId()).Num(), 1);
		TestFalse(TEXT("Turning isn't available"), Planner->IsMoveAvailable(L));
		Planner->SetEditingScript(true);
		TestTrue(TEXT("Scripted turtles aren't limited"), Planner->IsMoveAvailable(L));
	}

	// One ship per turn.
	{
		ULevelSave* const Puzzle = MakeRulesPuzzle();
		Puzzle->MaxTurtlesMovedPerTurn = 1;
		UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
		Session->LoadPuzzleFromLevelSave(Puzzle);

		UGatherAndHuntTurnPlanner* const Planner = NewObject<UGatherAndHuntTurnPlanner>();
		Planner->BindToSession(Session);
		Planner->SelectTurtle(1);
		Planner->AddMove(F);
		TestTrue(TEXT("Moving one ship is fine"), Planner->GetValidation().bIsValid);
		Planner->SelectTurtle(2);
		Planner->AddMove(F);
		TestFalse(TEXT("The planner flags a second ship"), Planner->GetValidation().bIsValid);
		TestEqual(TEXT("The second ship is the offender"), Planner->GetValidation().OffendingTurtleId, 2);

		FGatherAndHuntPuzzlePlanValidation Validation;
		TestFalse(TEXT("The session refuses two ships"), Session->SubmitSoloTurnPlan(MakePlan({{1, {F}}, {2, {F}}}), Validation));
		TestTrue(TEXT("The session takes one"), Session->SubmitSoloTurnPlan(MakePlan({{2, {F}}}), Validation));
	}

	// Reach vs deliver.
	for (const EGatherAndHuntPuzzleGoal Goal : {EGatherAndHuntPuzzleGoal::Reach, EGatherAndHuntPuzzleGoal::Deliver})
	{
		ULevelSave* const Puzzle = MakeRulesPuzzle();
		Puzzle->Goal = Goal;
		UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
		Session->LoadPuzzleFromLevelSave(Puzzle);
		FGatherAndHuntPuzzlePlanValidation Validation;
		Session->SubmitSoloTurnPlan(MakePlan({{1, {F, F}}}), Validation);
		TestEqual(Goal == EGatherAndHuntPuzzleGoal::Reach ? TEXT("Touching the ball wins a reach puzzle") : TEXT("Touching isn't enough to deliver"),
			Session->GetOutcome(), Goal == EGatherAndHuntPuzzleGoal::Reach ? EGatherAndHuntPuzzleOutcome::Victory : EGatherAndHuntPuzzleOutcome::InProgress);
	}

	// The rules survive a save and load.
	{
		ULevelSave* const Puzzle = MakeRulesPuzzle();
		Puzzle->Goal = EGatherAndHuntPuzzleGoal::Reach;
		Puzzle->AllowedMoveMask = 0x07;
		Puzzle->bShowGround = true;
		Puzzle->MaxTurtlesMovedPerTurn = 1;
		const FString File = TEXT("Automation/PuzzleRulesRoundTrip");
		TestTrue(TEXT("Saves"), UGatherAndHuntLevelEditorLibrary::SaveLevelSaveToProjectFile(Puzzle, File));
		const ULevelSave* const Loaded = UGatherAndHuntLevelEditorLibrary::LoadLevelSaveFromProjectFile(GetTransientPackage(), File);
		if (TestNotNull(TEXT("Loads"), Loaded))
		{
			TestEqual(TEXT("Goal kept"), Loaded->Goal, EGatherAndHuntPuzzleGoal::Reach);
			TestEqual(TEXT("Moves kept"), Loaded->AllowedMoveMask, 0x07);
			TestTrue(TEXT("Ground kept"), Loaded->bShowGround);
			TestEqual(TEXT("Ships per turn kept"), Loaded->MaxTurtlesMovedPerTurn, 1);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntTutorialSolutionsTest,
	"GatherAndHunt.GatherAndHunt.PuzzleRules.TutorialSolutions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntTutorialSolutionsTest::RunTest(const FString&)
{
	// Each tutorial puzzle's intended solution (one string per turn, for turtle 1),
	// and a play that must not win when the lesson depends on it.
	struct FSolution
	{
		const TCHAR* File;
		TArray<const TCHAR*> Turns;
		const TCHAR* MustFail;
	};
	const FSolution Solutions[] = {
		{TEXT("Act1/02_Turning"), {TEXT("FFLFF")}, nullptr},
		{TEXT("Act1/03_Obstacle"), {TEXT("FLFRFFFRF")}, TEXT("FFFF")},
		{TEXT("Act1/04_Gathering"), {TEXT("LFRFFFLFF"), TEXT("RFFLFLFFFF"), TEXT("RF")}, nullptr},
		{TEXT("Act1/05_UpHigh"), {TEXT("FFFUFF")}, nullptr},
		// Turning next to the chaser gets you caught; turn away from it first.
		{TEXT("Act1/06_TheChaser"), {TEXT("LFFFFRFFFF")}, TEXT("FFFFLFFFF")},
	};

	for (const FSolution& Solution : Solutions)
	{
		UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
		if (!TestTrue(*FString::Printf(TEXT("%s loads"), Solution.File), Session->LoadPuzzleFromRelativeFilename(Solution.File)))
		{
			continue;
		}
		for (const TCHAR* const Turn : Solution.Turns)
		{
			FGatherAndHuntPuzzlePlanValidation Validation;
			TestTrue(*FString::Printf(TEXT("%s accepts turn %s"), Solution.File, Turn), Session->SubmitSoloTurnPlan(MakePlan({{1, ParseMoves(Turn)}}), Validation));
		}
		TestEqual(*FString::Printf(TEXT("%s is solved by its solution"), Solution.File), Session->GetOutcome(), EGatherAndHuntPuzzleOutcome::Victory);

		if (Solution.MustFail != nullptr)
		{
			Session->ResetToInitialState();
			FGatherAndHuntPuzzlePlanValidation Validation;
			Session->SubmitSoloTurnPlan(MakePlan({{1, ParseMoves(Solution.MustFail)}}), Validation);
			TestNotEqual(*FString::Printf(TEXT("%s isn't won by %s"), Solution.File, Solution.MustFail), Session->GetOutcome(), EGatherAndHuntPuzzleOutcome::Victory);
		}
	}
	return true;
}

#endif
