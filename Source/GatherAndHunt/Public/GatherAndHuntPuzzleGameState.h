#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPuzzleGameState.generated.h"

class UGatherAndHuntPuzzleSession;

UCLASS()
class GATHERANDHUNT_API AGatherAndHuntPuzzleGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	void ApplySessionState(const UGatherAndHuntPuzzleSession* PuzzleSession);

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	bool bPuzzleLoaded = false;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	FString PuzzleName;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	int32 TurnNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	int32 PlayerScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	int32 TargetScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	int32 TargetTurnCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	int32 TargetObjectiveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	EGatherAndHuntPuzzleOutcome Outcome = EGatherAndHuntPuzzleOutcome::NotLoaded;
};
