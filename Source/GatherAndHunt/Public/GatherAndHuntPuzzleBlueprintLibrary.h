#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPuzzleBlueprintLibrary.generated.h"

class AGatherAndHuntPuzzleGameMode;
class UGatherAndHuntPuzzleSession;
class UObject;

UCLASS()
class GATHERANDHUNT_API UGatherAndHuntPuzzleBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle", meta = (WorldContext = "WorldContextObject"))
	static AGatherAndHuntPuzzleGameMode* GetPuzzleGameMode(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle", meta = (WorldContext = "WorldContextObject"))
	static UGatherAndHuntPuzzleSession* GetPuzzleSession(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	static FGatherAndHuntPuzzleTurnPlan MakeEmptyTurnPlan();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	static void AddTurtlePlanMove(
		UPARAM(ref) FGatherAndHuntPuzzleTurnPlan& TurnPlan,
		int32 TurtleId,
		EGatherAndHuntPuzzleMoveKind MoveKind);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle", meta = (WorldContext = "WorldContextObject"))
	static bool SubmitTurnPlanFromWorld(
		const UObject* WorldContextObject,
		const FGatherAndHuntPuzzleTurnPlan& TurnPlan,
		FGatherAndHuntPuzzlePlanValidation& OutValidation);
};
