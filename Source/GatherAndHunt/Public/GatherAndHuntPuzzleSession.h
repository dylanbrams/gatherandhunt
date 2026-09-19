#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BoardState.h"
#include "Resolver.h"
#include "Validation.h"
#include "ULevelSave.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPuzzleSession.generated.h"

UCLASS(BlueprintType)
class GATHERANDHUNT_API UGatherAndHuntPuzzleSession : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	bool LoadPuzzleFromRelativeFilename(const FString& RelativeFilename);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	bool LoadPuzzleFromLevelSave(ULevelSave* LevelSave);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	bool ResetToInitialState();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle")
	bool SubmitSoloTurnPlan(const FGatherAndHuntPuzzleTurnPlan& TurnPlan, FGatherAndHuntPuzzlePlanValidation& OutValidation);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	bool IsPuzzleLoaded() const { return LoadedLevelSave != nullptr && bHasCurrentBoardState; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	EGatherAndHuntPuzzleOutcome GetOutcome() const { return Outcome; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	const FString& GetLoadedPuzzleName() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	int32 GetCurrentTurnNumber() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	int32 GetPlayerScore() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	int32 GetTargetScore() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	int32 GetTargetTurnCount() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	int32 GetTargetObjectiveCount() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	const FString& GetIntroText() const;

	// World units per grid cell for the loaded puzzle (world = grid * cell size).
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	float GetGridCellSize() const { return LoadedLevelSave != nullptr ? LoadedLevelSave->GridCellSize : 100.0f; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	FGatherAndHuntPuzzleBoardSnapshot GetCurrentBoardSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	FGatherAndHuntPuzzleBoardSnapshot GetLastPlaybackStartBoardSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	FGatherAndHuntPuzzleTurnResultView GetLastTurnResultView() const;

	// --- Scripted opponent authoring ---
	// Turn numbers are the 0-based board turn the plan is resolved on.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle|Authoring")
	bool HasScriptedOpponentTurn(int32 TurnNumber) const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle|Authoring")
	FGatherAndHuntPuzzleTurnPlan GetScriptedOpponentTurn(int32 TurnNumber) const;

	// Replaces the scripted plan for TurnNumber; an empty plan removes it. Only
	// the loaded copy changes until SavePuzzleToProjectFile.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Authoring")
	bool SetScriptedOpponentTurn(int32 TurnNumber, const FGatherAndHuntPuzzleTurnPlan& TurnPlan);

	// Writes the loaded puzzle (including scripted turns) under Content/BoardSetups.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Authoring")
	bool SavePuzzleToProjectFile(const FString& RelativeFilename);

	// True when scripted turns changed since the puzzle was loaded or saved.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle|Authoring")
	bool HasUnsavedChanges() const { return bHasUnsavedChanges; }

	// C++ access for tools that run the resolver locally (e.g. the turn planner's preview).
	const turtlesim::BoardState* GetCurrentBoardState() const { return bHasCurrentBoardState ? &CurrentBoardState : nullptr; }

	// The scripted opponent plan that will be resolved against the player's next submission.
	turtlesim::TurnPlan BuildOpponentPlanForCurrentTurn() const;

	static turtlesim::TurnPlan ConvertTurnPlan(const FGatherAndHuntPuzzleTurnPlan& TurnPlan);
	static FGatherAndHuntPuzzlePlanValidation BuildValidation(const turtlesim::PlanValidation& Validation);
	static FIntVector ToFIntVector(const turtlesim::IntVec3& Position);

private:
	static bool ConvertLevelSaveToBoardState(const ULevelSave* LevelSave, turtlesim::BoardState& OutBoardState);
	static turtlesim::TurnPlan ConvertScriptedOpponentTurn(const FScriptedOpponentTurnData& TurnData);
	static const FScriptedOpponentTurnData* FindScriptedOpponentTurn(const ULevelSave* LevelSave, int32 TurnNumber);
	static EGatherAndHuntPuzzleEventType ToPuzzleEventType(turtlesim::EEventType EventType);
	static FGatherAndHuntPuzzleBoardSnapshot BuildBoardSnapshot(const turtlesim::BoardState& BoardState);
	static FGatherAndHuntPuzzleTurnResultView BuildTurnResultView(const turtlesim::TurnResult& TurnResult);
	static turtlesim::Orientation ConvertWorldRotationToOrientation(const FRotator& Rotation);
	static turtlesim::IntVec3 ConvertWorldPositionToGridPosition(const FVector& Position, float GridCellSize);
	static int32 ConvertHazardStepDistanceToCells(float HazardStepDistance, float GridCellSize);

	void EvaluateOutcome();

	UPROPERTY()
	TObjectPtr<ULevelSave> LoadedLevelSave;

	turtlesim::BoardState InitialBoardState;
	turtlesim::BoardState CurrentBoardState;
	turtlesim::BoardState LastPlaybackStartBoardState;
	turtlesim::TurnResult LastTurnResult;
	bool bHasCurrentBoardState = false;
	bool bHasLastPlaybackStartBoardState = false;
	bool bHasLastTurnResult = false;
	EGatherAndHuntPuzzleOutcome Outcome = EGatherAndHuntPuzzleOutcome::NotLoaded;
	bool bHasUnsavedChanges = false;
};
