#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "PieceSaveData.h"

namespace
{
	FPieceSaveData MakeLinearHazard()
	{
		FPieceSaveData Data;
		Data.Position = FVector::ZeroVector;
		Data.Rotation = FRotator::ZeroRotator;
		Data.PieceType = PieceTypeEnum::BP_LinearPatrolHazard;
		Data.MovementType = MovementTypeEnum::BP_AbsoluteMoveLinearPatrol;
		Data.HazardStepDistance = 100.0f;
		Data.HazardPrimaryStepCount = 3;
		return Data;
	}

	FPieceSaveData MakeCircularHazard()
	{
		FPieceSaveData Data;
		Data.Position = FVector::ZeroVector;
		Data.Rotation = FRotator::ZeroRotator;
		Data.PieceType = PieceTypeEnum::BP_CircularHazard;
		Data.MovementType = MovementTypeEnum::BP_AbsoluteMoveCircle;
		Data.HazardStepDistance = 100.0f;
		Data.HazardPrimaryStepCount = 2;
		Data.HazardSecondaryStepCount = 1;
		return Data;
	}

	FPieceSaveData MakeVerticalCircularHazard()
	{
		FPieceSaveData Data = MakeCircularHazard();
		Data.Rotation = FRotator(90.0f, 0.0f, 0.0f);
		return Data;
	}

	FPieceSaveData MakeVerticalLinearHazard()
	{
		FPieceSaveData Data = MakeLinearHazard();
		Data.Rotation = FRotator(90.0f, 0.0f, 0.0f);
		return Data;
	}

	FPieceSaveData MakeOffAxisLinearHazard()
	{
		FPieceSaveData Data = MakeLinearHazard();
		Data.Rotation = FRotator(45.0f, 45.0f, 0.0f);
		return Data;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntLinearHazardMotionTest,
	"GatherAndHunt.GatherAndHunt.Hazards.LinearPatrolMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntLinearHazardMotionTest::RunTest(const FString&)
{
	const FPieceSaveData Data = MakeLinearHazard();

	TestEqual(TEXT("Turn 0 starts at origin"), Data.GetDeterministicHazardPositionAtTurn(0), FVector(0.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 1 moves forward one step"), Data.GetDeterministicHazardPositionAtTurn(1), FVector(100.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 2 moves forward two steps"), Data.GetDeterministicHazardPositionAtTurn(2), FVector(200.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 3 reaches patrol end"), Data.GetDeterministicHazardPositionAtTurn(3), FVector(300.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 4 reverses direction"), Data.GetDeterministicHazardPositionAtTurn(4), FVector(200.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 5 continues back"), Data.GetDeterministicHazardPositionAtTurn(5), FVector(100.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 6 loops to origin"), Data.GetDeterministicHazardPositionAtTurn(6), FVector(0.0f, 0.0f, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntVerticalLinearHazardMotionTest,
	"GatherAndHunt.GatherAndHunt.Hazards.VerticalLinearPatrolMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntVerticalLinearHazardMotionTest::RunTest(const FString&)
{
	const FPieceSaveData Data = MakeVerticalLinearHazard();

	TestEqual(TEXT("Turn 0 starts at origin"), Data.GetDeterministicHazardPositionAtTurn(0), FVector(0.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 1 moves upward"), Data.GetDeterministicHazardPositionAtTurn(1), FVector(0.0f, 0.0f, 100.0f));
	TestEqual(TEXT("Turn 2 continues upward"), Data.GetDeterministicHazardPositionAtTurn(2), FVector(0.0f, 0.0f, 200.0f));
	TestEqual(TEXT("Turn 3 reaches vertical patrol end"), Data.GetDeterministicHazardPositionAtTurn(3), FVector(0.0f, 0.0f, 300.0f));
	TestEqual(TEXT("Turn 4 reverses downward"), Data.GetDeterministicHazardPositionAtTurn(4), FVector(0.0f, 0.0f, 200.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntOffAxisLinearHazardMotionTest,
	"GatherAndHunt.GatherAndHunt.Hazards.OffAxisLinearPatrolSnapsToAxis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntOffAxisLinearHazardMotionTest::RunTest(const FString&)
{
	const FPieceSaveData Data = MakeOffAxisLinearHazard();

	TestEqual(TEXT("Turn 0 starts at origin"), Data.GetDeterministicHazardPositionAtTurn(0), FVector::ZeroVector);
	TestEqual(TEXT("Turn 1 snaps to a readable axis-aligned patrol"), Data.GetDeterministicHazardPositionAtTurn(1), FVector(0.0f, 0.0f, 100.0f));
	TestEqual(TEXT("Turn 2 continues on that axis"), Data.GetDeterministicHazardPositionAtTurn(2), FVector(0.0f, 0.0f, 200.0f));
	TestEqual(TEXT("Turn 3 remains axis-aligned"), Data.GetDeterministicHazardPositionAtTurn(3), FVector(0.0f, 0.0f, 300.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntCircularHazardMotionTest,
	"GatherAndHunt.GatherAndHunt.Hazards.CircularMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntCircularHazardMotionTest::RunTest(const FString&)
{
	const FPieceSaveData Data = MakeCircularHazard();

	TestEqual(TEXT("Turn 0 starts at origin"), Data.GetDeterministicHazardPositionAtTurn(0), FVector(0.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 1 moves along right edge"), Data.GetDeterministicHazardPositionAtTurn(1), FVector(0.0f, 100.0f, 0.0f));
	TestEqual(TEXT("Turn 2 reaches top-right corner"), Data.GetDeterministicHazardPositionAtTurn(2), FVector(100.0f, 100.0f, 0.0f));
	TestEqual(TEXT("Turn 3 reaches top-left corner"), Data.GetDeterministicHazardPositionAtTurn(3), FVector(200.0f, 100.0f, 0.0f));
	TestEqual(TEXT("Turn 4 moves down left edge"), Data.GetDeterministicHazardPositionAtTurn(4), FVector(200.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 5 returns toward origin"), Data.GetDeterministicHazardPositionAtTurn(5), FVector(100.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 6 loops to origin"), Data.GetDeterministicHazardPositionAtTurn(6), FVector(0.0f, 0.0f, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntHazardPhaseOffsetTest,
	"GatherAndHunt.GatherAndHunt.Hazards.PhaseOffset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntHazardPhaseOffsetTest::RunTest(const FString&)
{
	FPieceSaveData Data = MakeLinearHazard();
	Data.HazardPhaseOffset = 2;

	TestEqual(TEXT("Phase offset advances the sampled path"), Data.GetDeterministicHazardPositionAtTurn(0), FVector(0.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 1 respects authored phase offset"), Data.GetDeterministicHazardPositionAtTurn(1), FVector(300.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 2 continues the wrapped pattern"), Data.GetDeterministicHazardPositionAtTurn(2), FVector(200.0f, 0.0f, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntVerticalCircularHazardMotionTest,
	"GatherAndHunt.GatherAndHunt.Hazards.VerticalCircularMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntVerticalCircularHazardMotionTest::RunTest(const FString&)
{
	const FPieceSaveData Data = MakeVerticalCircularHazard();

	TestEqual(TEXT("Turn 0 starts at origin"), Data.GetDeterministicHazardPositionAtTurn(0), FVector(0.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Turn 1 moves on rotated right axis"), Data.GetDeterministicHazardPositionAtTurn(1), FVector(0.0f, 100.0f, 0.0f));
	TestEqual(TEXT("Turn 2 enters vertical rise"), Data.GetDeterministicHazardPositionAtTurn(2), FVector(0.0f, 100.0f, 100.0f));
	TestEqual(TEXT("Turn 3 reaches top of vertical loop"), Data.GetDeterministicHazardPositionAtTurn(3), FVector(0.0f, 100.0f, 200.0f));
	TestEqual(TEXT("Turn 4 crosses back on upper edge"), Data.GetDeterministicHazardPositionAtTurn(4), FVector(0.0f, 0.0f, 200.0f));
	TestEqual(TEXT("Turn 5 descends toward origin"), Data.GetDeterministicHazardPositionAtTurn(5), FVector(0.0f, 0.0f, 100.0f));
	TestEqual(TEXT("Turn 6 closes the loop"), Data.GetDeterministicHazardPositionAtTurn(6), FVector(0.0f, 0.0f, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntNonHazardMotionTest,
	"GatherAndHunt.GatherAndHunt.Hazards.NonHazardStaysPut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntNonHazardMotionTest::RunTest(const FString&)
{
	FPieceSaveData Data;
	Data.Position = FVector(25.0f, 50.0f, 75.0f);
	Data.PieceType = PieceTypeEnum::BP_Objective;
	Data.MovementType = MovementTypeEnum::None;

	TestEqual(TEXT("Non-hazard pieces are not moved by hazard logic"), Data.GetDeterministicHazardPositionAtTurn(10), FVector(25.0f, 50.0f, 75.0f));
	return true;
}

#endif
