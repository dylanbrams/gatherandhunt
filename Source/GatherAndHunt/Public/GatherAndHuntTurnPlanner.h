#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BoardState.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntTurnPlanner.generated.h"

class UGatherAndHuntPuzzleSession;

UENUM(BlueprintType)
enum class EGatherAndHuntPlannerStepKind : uint8
{
	Start,
	Moved,
	Turned,
	Waited,
	Blocked,
	Destroyed,
	Delivered
};

// One tick of a turtle's previewed path. Mirrors turtlesim::TrackStep.
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPlannerTrackStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 TickIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	EGatherAndHuntPlannerStepKind Kind = EGatherAndHuntPlannerStepKind::Start;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	FIntVector GridPosition = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 Heading = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 Pitch = 1;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	bool bClaimedObjective = false;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 PointsDelta = 0;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPlannerTurtleTrack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 TurtleId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 OwnerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	TArray<FGatherAndHuntPlannerTrackStep> Steps;
};

// Per-turtle row for the planner's turtle list and cost breakdown.
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPlannerTurtleSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 TurtleId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	FIntVector GridPosition = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 Heading = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 Pitch = 1;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 CarriedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 MoveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 PlanCost = 0;

	// 0 = no moves; otherwise the most expensive cost tier this plan reaches (1-3).
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 HighestCostTier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	bool bHasValidationError = false;

	// Last step of this turtle's previewed path (Destroyed / Delivered / etc.).
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	EGatherAndHuntPlannerStepKind PreviewFate = EGatherAndHuntPlannerStepKind::Start;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 PreviewClaimedPoints = 0;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPlannerPreviewSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 ScoreBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 ScoreAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 ObjectivesClaimed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 TurtlesDelivered = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 TurtlesLost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Planner")
	int32 TickCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGatherAndHuntPlannerChanged);

// Editable turn plan for the local player, with live cost, validation and a
// resolver-backed preview (spec §16). Holds no gameplay authority: validation
// and preview call the same TurtleSim functions the session uses on submit.
UCLASS(BlueprintType)
class GATHERANDHUNT_API UGatherAndHuntTurnPlanner : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void BindToSession(UGatherAndHuntPuzzleSession* InSession);

	// Re-reads the board from the session (after a submit, reset or load). Keeps
	// queued moves for turtles that are still playable.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void RefreshFromSession();

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	bool HasBoard() const { return bHasBoard; }

	// --- Selection ---
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void SelectTurtle(int32 TurtleId);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void SelectAdjacentTurtle(int32 Direction);

	// INDEX_NONE puts the cursor after the last move (new moves append).
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void SelectMove(int32 MoveIndex);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	int32 GetSelectedTurtleId() const { return SelectedTurtleId; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	int32 GetSelectedMoveIndex() const { return SelectedMoveIndex; }

	// --- Editing the selected turtle's queue ---
	// Inserts after the selected move (or appends) and selects the new move.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void AddMove(EGatherAndHuntPuzzleMoveKind MoveKind);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void RemoveMove(int32 MoveIndex);

	// Removes the selected move, or the last one if none is selected.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void RemoveSelectedMove();

	// Moves the selected move one slot left (-1) or right (+1).
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void ShiftSelectedMove(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void ClearSelectedTurtle();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void ClearAll();

	// --- Queries ---
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	TArray<EGatherAndHuntPuzzleMoveKind> GetTurtleMoves(int32 TurtleId) const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	TArray<FGatherAndHuntPlannerTurtleSummary> GetTurtleSummaries() const { return TurtleSummaries; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	FGatherAndHuntPuzzleTurnPlan GetTurnPlan() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	int32 GetTotalCost() const { return TotalCost; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	int32 GetBudget() const { return Budget; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	FGatherAndHuntPuzzlePlanValidation GetValidation() const { return Validation; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	bool CanSubmit() const { return bHasBoard && Validation.bIsValid; }

	// Cost of the move at MoveIndex (0-based) within one turtle's queue.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	static int32 GetMoveCost(int32 MoveIndex);

	// Cost tier (1-3) of the move at MoveIndex.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	static int32 GetMoveCostTier(int32 MoveIndex);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	TArray<FGatherAndHuntPlannerTurtleTrack> GetPreviewTracks() const { return PreviewTracks; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	FGatherAndHuntPlannerPreviewSummary GetPreviewSummary() const { return PreviewSummary; }

	// When true the preview resolves the puzzle's scripted opponent turn too;
	// when false it shows the "uncontested" result (spec §16.2).
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void SetPreviewIncludesOpponent(bool bInclude);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	bool GetPreviewIncludesOpponent() const { return bPreviewIncludesOpponent; }

	// Submits through the puzzle GameMode when present (so GameState stays in
	// sync), otherwise straight to the session. Clears the plan on success.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	bool Submit(FGatherAndHuntPuzzlePlanValidation& OutValidation);

	// Fires after any edit, selection change or refresh.
	UPROPERTY(BlueprintAssignable, Category = "GatherAndHunt|Planner")
	FGatherAndHuntPlannerChanged OnPlanChanged;

	// Fires after a plan is accepted; the session holds the new turn result.
	UPROPERTY(BlueprintAssignable, Category = "GatherAndHunt|Planner")
	FGatherAndHuntPlannerChanged OnPlanSubmitted;

	// Total cost units per turn, shared across the player's turtles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Planner")
	int32 Budget = 20;

	// Puzzle mode: the human player is always player 0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Planner")
	int32 PlayerId = 0;

private:
	void Recompute();
	TArray<EGatherAndHuntPuzzleMoveKind>* FindSelectedMoves();

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntPuzzleSession> Session;

	turtlesim::BoardState Board;
	bool bHasBoard = false;

	// Playable turtles owned by PlayerId, ascending id.
	TArray<int32> PlayerTurtleIds;
	TMap<int32, TArray<EGatherAndHuntPuzzleMoveKind>> MovesByTurtle;
	int32 SelectedTurtleId = INDEX_NONE;
	int32 SelectedMoveIndex = INDEX_NONE;
	bool bPreviewIncludesOpponent = true;

	int32 TotalCost = 0;
	FGatherAndHuntPuzzlePlanValidation Validation;
	TArray<FGatherAndHuntPlannerTurtleSummary> TurtleSummaries;
	TArray<FGatherAndHuntPlannerTurtleTrack> PreviewTracks;
	FGatherAndHuntPlannerPreviewSummary PreviewSummary;
};
