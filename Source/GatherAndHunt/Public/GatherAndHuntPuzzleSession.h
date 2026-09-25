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

	// --- Puzzle rules (see ULevelSave) ---
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	EGatherAndHuntPuzzleGoal GetGoal() const { return LoadedLevelSave != nullptr ? LoadedLevelSave->Goal : EGatherAndHuntPuzzleGoal::Deliver; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	int32 GetAllowedMoveMask() const { return LoadedLevelSave != nullptr ? LoadedLevelSave->AllowedMoveMask : GatherAndHuntAllMovesMask; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	bool IsMoveAllowed(EGatherAndHuntPuzzleMoveKind MoveKind) const { return IsGatherAndHuntMoveAllowed(GetAllowedMoveMask(), MoveKind); }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	bool ShowsGround() const { return LoadedLevelSave != nullptr && LoadedLevelSave->bShowGround; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	int32 GetMaxTurtlesMovedPerTurn() const { return LoadedLevelSave != nullptr ? LoadedLevelSave->MaxTurtlesMovedPerTurn : 0; }

	// Checks a player plan against the puzzle's rules (allowed moves, turtles
	// per turn). The sim's own checks (budget, ownership, pitch) are separate.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	FGatherAndHuntPuzzlePlanValidation CheckPuzzleRules(const FGatherAndHuntPuzzleTurnPlan& PlayerPlan) const;

	// Player, Ally or Opponent. Unknown ids report Player.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle")
	EGatherAndHuntTurtleRole GetTurtleRole(int32 TurtleId) const;

	// --- Scripted turn authoring ---
	// A scripted turn holds the moves of every script-controlled turtle (allies
	// and opponents) for one board turn. Turn numbers are 0-based board turns.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle|Authoring")
	bool HasScriptedTurn(int32 TurnNumber) const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle|Authoring")
	FGatherAndHuntPuzzleTurnPlan GetScriptedTurn(int32 TurnNumber) const;

	// Replaces the scripted plan for TurnNumber; an empty plan removes it. Only
	// the loaded copy changes until SavePuzzleToProjectFile.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Authoring")
	bool SetScriptedTurn(int32 TurnNumber, const FGatherAndHuntPuzzleTurnPlan& TurnPlan);

	// Writes the loaded puzzle (including scripted turns) under Content/BoardSetups.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Authoring")
	bool SavePuzzleToProjectFile(const FString& RelativeFilename);

	// True when the puzzle changed since it was loaded or saved.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle|Authoring")
	bool HasUnsavedChanges() const { return bHasUnsavedChanges; }

	// Replaces the puzzle with an edited copy of it (layout, metadata, scripts),
	// restarting at turn 0 and marking the puzzle unsaved. The session keeps its
	// own copy.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Authoring")
	bool ApplyAuthoredLevelSave(ULevelSave* LevelSave);

	// The loaded puzzle as authored. Edit a copy and hand it to ApplyAuthoredLevelSave.
	const ULevelSave* GetLevelSave() const { return LoadedLevelSave; }

	// Board-file rotation to sim facing. A vertical facing takes its heading from the yaw.
	static turtlesim::Orientation ConvertWorldRotationToOrientation(const FRotator& Rotation);

	// C++ access for tools that run the resolver locally (e.g. the turn planner's preview).
	const turtlesim::BoardState* GetCurrentBoardState() const { return bHasCurrentBoardState ? &CurrentBoardState : nullptr; }

	// This turn's scripted moves, split by team: allies join the player's plan,
	// opponents form the other plan.
	void BuildScriptedPlansForCurrentTurn(turtlesim::TurnPlan& OutAllyPlan, turtlesim::TurnPlan& OutOpponentPlan) const;

	static turtlesim::TurnPlan ConvertTurnPlan(const FGatherAndHuntPuzzleTurnPlan& TurnPlan);
	static FGatherAndHuntPuzzlePlanValidation BuildValidation(const turtlesim::PlanValidation& Validation);
	static FIntVector ToFIntVector(const turtlesim::IntVec3& Position);

private:
	static bool ConvertLevelSaveToBoardState(const ULevelSave* LevelSave, turtlesim::BoardState& OutBoardState);
	static const FScriptedOpponentTurnData* FindScriptedOpponentTurn(const ULevelSave* LevelSave, int32 TurnNumber);
	static EGatherAndHuntPuzzleEventType ToPuzzleEventType(turtlesim::EEventType EventType);
	FGatherAndHuntPuzzleBoardSnapshot BuildBoardSnapshot(const turtlesim::BoardState& BoardState) const;
	FGatherAndHuntPuzzleTurnResultView BuildTurnResultView(const turtlesim::TurnResult& TurnResult) const;
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

	// Player-team turtles moved by the script rather than the player.
	TSet<int32> ScriptedAllyIds;
};
