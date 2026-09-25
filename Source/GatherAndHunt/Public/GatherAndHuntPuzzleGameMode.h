#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPuzzleGameMode.generated.h"

class AGatherAndHuntPuzzleGameState;
class UGatherAndHuntPuzzleSession;

UCLASS()
class GATHERANDHUNT_API AGatherAndHuntPuzzleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGatherAndHuntPuzzleGameMode();

	// Accepts "?Puzzle=<file under Content/BoardSetups>" to override
	// AutoLoadPuzzleRelativeFilename, and "?Edit" to open in the board editor.
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	bool LoadPuzzleFromRelativeFilename(const FString& RelativeFilename);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	bool SubmitSoloTurnPlan(const FGatherAndHuntPuzzleTurnPlan& TurnPlan, FGatherAndHuntPuzzlePlanValidation& OutValidation);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	bool ResetPuzzle();

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	UGatherAndHuntPuzzleSession* GetPuzzleSession() const { return PuzzleSession; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Puzzle")
	FString AutoLoadPuzzleRelativeFilename;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Puzzle")
	bool bAutoLoadPuzzleOnBeginPlay = true;

	// The HUD opens the board editor instead of the planner. If the puzzle file
	// doesn't exist yet it starts as a new, empty board.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Puzzle")
	bool bStartInBoardEditor = false;

private:
	void RefreshGameStateFromSession();
	AGatherAndHuntPuzzleGameState* GetPuzzleGameState() const;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntPuzzleSession> PuzzleSession;
};
