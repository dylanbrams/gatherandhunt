#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GatherAndHuntCampaign.h"
#include "GatherAndHuntLevelEditorLibrary.h"
#include "GatherAndHuntPuzzleSession.h"
#include "GatherAndHuntTurnPlanner.h"

namespace
{
	constexpr EGatherAndHuntPuzzleMoveKind F = EGatherAndHuntPuzzleMoveKind::Forward;
	constexpr EGatherAndHuntPuzzleMoveKind L = EGatherAndHuntPuzzleMoveKind::YawLeft;

	const TCHAR* const TestMapFile = TEXT("Automation/CampaignMap");
	const TCHAR* const TestSaveSlot = TEXT("CampaignAutomationTest");

	FPieceSaveData MakeMapPiece(const int32 Id, const PieceTypeEnum Type, const FIntVector& Cell)
	{
		FPieceSaveData Data;
		Data.PieceSaveID = Id;
		Data.PieceType = Type;
		Data.Position = FVector(Cell) * 100.0f;
		return Data; // Rotation zero: faces East (+X).
	}

	// 5x3x1: the ship at (0,1), a block at (3,1). Markers: A straight ahead at
	// (2,1), B round the corner at (2,2).
	FGatherAndHuntCampaignDefinition MakeTestCampaign()
	{
		ULevelSave* const Map = UGatherAndHuntLevelEditorLibrary::CreateEmptyLevelSave(GetTransientPackage(), TEXT("Campaign test map"), 0, FVector(400.0f, 200.0f, 1.0f));
		Map->PieceSaveDataArray.Add(MakeMapPiece(1, PieceTypeEnum::BP_PlayerOwnedPiece, FIntVector(0, 1, 0)));
		Map->PieceSaveDataArray.Add(MakeMapPiece(2, PieceTypeEnum::BP_Hazard, FIntVector(3, 1, 0)));
		// A ball on the map board itself isn't a level and must be dropped.
		Map->PieceSaveDataArray.Add(MakeMapPiece(3, PieceTypeEnum::BP_Objective, FIntVector(1, 1, 0)));
		UGatherAndHuntLevelEditorLibrary::SaveLevelSaveToProjectFile(Map, TestMapFile);

		FGatherAndHuntCampaignDefinition Definition;
		Definition.MapBoard = TestMapFile;
		Definition.StartingMoves = 2;
		Definition.StartingMotions = {F};

		FGatherAndHuntCampaignNode& A = Definition.Nodes.AddDefaulted_GetRef();
		A.Id = TEXT("A");
		A.Cell = FIntVector(2, 1, 0);
		A.Puzzle = TEXT("Act1/03_Obstacle");
		A.Reward.Moves = 2;
		A.Reward.Motions = {L};

		FGatherAndHuntCampaignNode& B = Definition.Nodes.AddDefaulted_GetRef();
		B.Id = TEXT("B");
		B.Cell = FIntVector(2, 2, 0);
		B.Puzzle = TEXT("Act1/02_Turning");
		return Definition;
	}

	FGatherAndHuntPuzzleTurnPlan MakeShipPlan(const TArray<EGatherAndHuntPuzzleMoveKind>& Moves)
	{
		FGatherAndHuntPuzzleTurnPlan Plan;
		FGatherAndHuntPuzzleTurtlePlan& ShipPlan = Plan.TurtlePlans.AddDefaulted_GetRef();
		ShipPlan.TurtleId = 1;
		ShipPlan.Moves = Moves;
		return Plan;
	}

	// The map as the map GameMode sets it up.
	UGatherAndHuntPuzzleSession* StartMapSession(UGatherAndHuntCampaign* Campaign)
	{
		FString Error;
		UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
		Session->LoadPuzzleFromLevelSave(Campaign->BuildMapLevelSave(GetTransientPackage(), Error));
		Session->SetMaxMovesPerTurn(Campaign->GetAllowance().Moves);
		return Session;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntCampaignProgressTest,
	"GatherAndHunt.GatherAndHunt.Campaign.ProgressAndAllowance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntCampaignProgressTest::RunTest(const FString&)
{
	const FGatherAndHuntCampaignDefinition Definition = MakeTestCampaign();
	UGatherAndHuntCampaign* const Campaign = UGatherAndHuntCampaign::Create(GetTransientPackage(), Definition, TestSaveSlot);
	Campaign->ResetProgress();

	TestEqual(TEXT("Starts with the starting moves"), Campaign->GetAllowance().Moves, 2);
	TestEqual(TEXT("Starts with the starting motions"), Campaign->GetAllowance().MoveMask, 1 << static_cast<int32>(F));

	TestFalse(TEXT("An unknown level can't be finished"), Campaign->CompleteNode(TEXT("Nope")));
	TestTrue(TEXT("Finishing a level is new the first time"), Campaign->CompleteNode(TEXT("A")));
	TestFalse(TEXT("Replaying it earns nothing"), Campaign->CompleteNode(TEXT("A")));
	TestTrue(TEXT("The level shows as finished"), Campaign->IsNodeCompleted(0));
	TestEqual(TEXT("Its reward adds moves"), Campaign->GetAllowance().Moves, 4);
	TestTrue(TEXT("Its reward adds motions"), IsGatherAndHuntMoveAllowed(Campaign->GetAllowance().MoveMask, L));

	const UGatherAndHuntCampaign* const Reloaded = UGatherAndHuntCampaign::Create(GetTransientPackage(), Definition, TestSaveSlot);
	TestTrue(TEXT("Progress is stored in the save slot"), Reloaded->IsNodeCompleted(0));
	TestEqual(TEXT("The stored progress gives the same allowance"), Reloaded->GetAllowance().Moves, 4);

	Campaign->ResetProgress();
	TestEqual(TEXT("Reset goes back to the starting moves"), Campaign->GetAllowance().Moves, 2);
	TestFalse(TEXT("Reset clears the save slot"),
		UGatherAndHuntCampaign::Create(GetTransientPackage(), Definition, TestSaveSlot)->IsNodeCompleted(0));

	TestEqual(TEXT("Rewards read as text"), UGatherAndHuntCampaign::DescribeReward(Definition.Nodes[0].Reward), FString(TEXT("+2 moves, YAW L")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntCampaignMapTest,
	"GatherAndHunt.GatherAndHunt.Campaign.MapSorties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntCampaignMapTest::RunTest(const FString&)
{
	UGatherAndHuntCampaign* const Campaign = UGatherAndHuntCampaign::Create(GetTransientPackage(), MakeTestCampaign());
	TArray<FString> Issues;
	TestTrue(TEXT("The test campaign is well formed"), Campaign->Validate(Issues));

	// Two moves, forward only.
	{
		UGatherAndHuntPuzzleSession* const Session = StartMapSession(Campaign);
		TestEqual(TEXT("The map has a ball per marker and nothing else"), Session->GetCurrentBoardSnapshot().Objectives.Num(), 2);
		TestEqual(TEXT("The move cap replaces the cost budget"), Session->GetMoveBudget(), MAX_int32);

		FGatherAndHuntPuzzlePlanValidation Validation;
		TestFalse(TEXT("A third move is refused"), Session->SubmitSoloTurnPlan(MakeShipPlan({F, F, F}), Validation));
		TestEqual(TEXT("The move past the cap is the offender"), Validation.OffendingMoveIndex, 2);
		TestFalse(TEXT("A motion that isn't unlocked is refused"), Session->SubmitSoloTurnPlan(MakeShipPlan({F, L}), Validation));

		UGatherAndHuntTurnPlanner* const Planner = NewObject<UGatherAndHuntTurnPlanner>();
		Planner->Budget = Session->GetMoveBudget();
		Planner->BindToSession(Session);
		TestFalse(TEXT("The planner doesn't offer a locked motion"), Planner->IsMoveAvailable(L));
		Planner->AddMove(F);
		Planner->AddMove(F);
		TestTrue(TEXT("The planner accepts a plan within the cap"), Planner->GetValidation().bIsValid);
		Planner->AddMove(F);
		TestFalse(TEXT("The planner flags a plan past the cap"), Planner->GetValidation().bIsValid);

		TestEqual(TEXT("Only the marker straight ahead is in range"), Campaign->FindReachableNodes().Num(), 1);
		TestTrue(TEXT("Marker A is in range"), Campaign->FindReachableNodes().Contains(0));

		TestTrue(TEXT("Two moves forward are accepted"), Session->SubmitSoloTurnPlan(MakeShipPlan({F, F}), Validation));
		TestEqual(TEXT("Touching marker A enters level A"), Campaign->FindEnteredNode(Session->GetLastTurnResultView()), 0);

		Session->ResetToInitialState();
		Session->SubmitSoloTurnPlan(MakeShipPlan({F}), Validation);
		TestEqual(TEXT("A flight that touches nothing enters nothing"), Campaign->FindEnteredNode(Session->GetLastTurnResultView()), static_cast<int32>(INDEX_NONE));
	}

	// Level A's reward: two more moves and a left turn.
	Campaign->CompleteNode(TEXT("A"));
	{
		UGatherAndHuntPuzzleSession* const Session = StartMapSession(Campaign);
		TestEqual(TEXT("Both markers are in range now"), Campaign->FindReachableNodes().Num(), 2);

		FGatherAndHuntPuzzlePlanValidation Validation;
		TestTrue(TEXT("The longer flight with a turn is accepted"), Session->SubmitSoloTurnPlan(MakeShipPlan({F, F, L, F}), Validation));
		TestEqual(TEXT("The last marker touched is the level entered"), Campaign->FindEnteredNode(Session->GetLastTurnResultView()), 1);

		Session->ResetToInitialState();
		Session->SubmitSoloTurnPlan(MakeShipPlan({F, F, F}), Validation);
		TestEqual(TEXT("A ship lost after touching a marker enters nothing"), Campaign->FindEnteredNode(Session->GetLastTurnResultView()), static_cast<int32>(INDEX_NONE));
	}

	// Authoring mistakes are reported.
	{
		FGatherAndHuntCampaignDefinition Broken = MakeTestCampaign();
		Broken.Nodes[1].Id = TEXT("A");
		Broken.Nodes[1].Cell = FIntVector(9, 9, 0);
		Broken.Nodes[0].Puzzle = TEXT("Automation/NoSuchPuzzle");
		Broken.Nodes[0].Cell = FIntVector(3, 1, 0);
		TestFalse(TEXT("A broken campaign doesn't validate"), UGatherAndHuntCampaign::Create(GetTransientPackage(), Broken)->Validate(Issues));
		TestEqual(TEXT("Duplicate id, off the map, missing puzzle, marker on a block"), Issues.Num(), 4);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntCampaignShippedTest,
	"GatherAndHunt.GatherAndHunt.Campaign.ShippedCampaignCompletable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The campaign in Content/BoardSetups/Campaign: it parses, its markers and
// puzzle files are sound, and playing the levels in the order listed never
// leaves the next one out of range.
bool FGatherAndHuntCampaignShippedTest::RunTest(const FString&)
{
	FString Error;
	// An empty save slot keeps the test away from the player's progress.
	UGatherAndHuntCampaign* const Campaign = UGatherAndHuntCampaign::Load(GetTransientPackage(), Error, FString());
	if (!TestNotNull(*FString::Printf(TEXT("The campaign loads (%s)"), *Error), Campaign))
	{
		return true;
	}

	TArray<FString> Issues;
	Campaign->Validate(Issues);
	for (const FString& Issue : Issues)
	{
		AddError(Issue);
	}

	for (int32 Index = 0; Index < Campaign->GetNodes().Num(); ++Index)
	{
		const FGatherAndHuntCampaignNode& Node = Campaign->GetNodes()[Index];
		const FGatherAndHuntCampaignAllowance Allowance = Campaign->GetAllowance();
		TestTrue(*FString::Printf(TEXT("%s is in range with %d moves (%s)"), *Node.Id, Allowance.Moves, *UGatherAndHuntCampaign::DescribeMotions(Allowance.MoveMask)),
			Campaign->FindReachableNodes().Contains(Index));
		Campaign->CompleteNode(Node.Id);
	}
	return true;
}

#endif
