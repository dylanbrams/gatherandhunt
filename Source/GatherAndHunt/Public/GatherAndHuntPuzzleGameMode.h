#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPuzzleGameMode.generated.h"

class AGatherAndHuntPuzzleGameState;
class UGatherAndHuntPuzzleSession;
class ULevelSave;

UCLASS()
class GATHERANDHUNT_API AGatherAndHuntPuzzleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGatherAndHuntPuzzleGameMode();

	// Accepts "?Puzzle=<file under Content/BoardSetups>" to override
	// AutoLoadPuzzleRelativeFilename, "?Edit" to open in the board editor, and
	// "?CampaignNode=<id>" when the level select map opens the puzzle.
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	bool LoadPuzzleFromRelativeFilename(const FString& RelativeFilename);

	// A puzzle built in memory rather than read from a board file.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	bool LoadPuzzleFromLevelSave(ULevelSave* LevelSave);

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

	// The level select marker this puzzle was opened from; empty when it was
	// opened from the menu. Winning records the level as finished there.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	FString CampaignNodeId;

	// What winning just unlocked on the level select map ("+2 moves, YAW L");
	// empty until then, and for a level that was already finished.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Puzzle")
	FString CampaignRewardText;

	// Whether the planner offers its authoring menu (board editor, script mode,
	// loading and saving board files). Always for a puzzle opened from the main
	// menu; for one opened from the level select map, once the map's first
	// level has been finished.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	bool AreAuthoringToolsAvailable() const { return bAuthoringToolsAvailable; }

private:
	void RecordCampaignWin();

	bool bAuthoringToolsAvailable = true;

	void RefreshGameStateFromSession();
	AGatherAndHuntPuzzleGameState* GetPuzzleGameState() const;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntPuzzleSession> PuzzleSession;
};
