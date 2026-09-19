#include "GatherAndHuntPuzzleBlueprintLibrary.h"

#include "Engine/World.h"
#include "GatherAndHuntPuzzleGameMode.h"
#include "GatherAndHuntPuzzleSession.h"

AGatherAndHuntPuzzleGameMode* UGatherAndHuntPuzzleBlueprintLibrary::GetPuzzleGameMode(const UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}

	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		return World->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>();
	}

	return nullptr;
}

UGatherAndHuntPuzzleSession* UGatherAndHuntPuzzleBlueprintLibrary::GetPuzzleSession(const UObject* WorldContextObject)
{
	if (AGatherAndHuntPuzzleGameMode* GameMode = GetPuzzleGameMode(WorldContextObject))
	{
		return GameMode->GetPuzzleSession();
	}

	return nullptr;
}

FGatherAndHuntPuzzleTurnPlan UGatherAndHuntPuzzleBlueprintLibrary::MakeEmptyTurnPlan()
{
	return FGatherAndHuntPuzzleTurnPlan{};
}

void UGatherAndHuntPuzzleBlueprintLibrary::AddTurtlePlanMove(
	FGatherAndHuntPuzzleTurnPlan& TurnPlan,
	const int32 TurtleId,
	const EGatherAndHuntPuzzleMoveKind MoveKind)
{
	for (FGatherAndHuntPuzzleTurtlePlan& TurtlePlan : TurnPlan.TurtlePlans)
	{
		if (TurtlePlan.TurtleId == TurtleId)
		{
			TurtlePlan.Moves.Add(MoveKind);
			return;
		}
	}

	FGatherAndHuntPuzzleTurtlePlan TurtlePlan;
	TurtlePlan.TurtleId = TurtleId;
	TurtlePlan.Moves.Add(MoveKind);
	TurnPlan.TurtlePlans.Add(TurtlePlan);
}

bool UGatherAndHuntPuzzleBlueprintLibrary::SubmitTurnPlanFromWorld(
	const UObject* WorldContextObject,
	const FGatherAndHuntPuzzleTurnPlan& TurnPlan,
	FGatherAndHuntPuzzlePlanValidation& OutValidation)
{
	if (AGatherAndHuntPuzzleGameMode* GameMode = GetPuzzleGameMode(WorldContextObject))
	{
		return GameMode->SubmitSoloTurnPlan(TurnPlan, OutValidation);
	}

	OutValidation = FGatherAndHuntPuzzlePlanValidation{};
	OutValidation.bIsValid = false;
	OutValidation.ErrorMessage = TEXT("Puzzle GameMode is not available.");
	return false;
}
