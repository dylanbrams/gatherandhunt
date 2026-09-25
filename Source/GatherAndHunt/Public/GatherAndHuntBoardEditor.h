#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GatherAndHuntLevelEditorLibrary.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntBoardEditor.generated.h"

class UGatherAndHuntPuzzleSession;
class ULevelSave;

// What the palette places. Each maps onto a PieceTypeEnum + MovementTypeEnum
// pair in the board file (see GetPieceKind / the .cpp's ApplyKind).
UENUM(BlueprintType)
enum class EGatherAndHuntBoardPieceKind : uint8
{
	Turtle,
	Objective,
	GoldObjective,
	Block,         // Static obstacle.
	Hunter,        // Chases the nearest turtle.
	PatrolHazard,  // Walks out and back along its facing.
	LoopHazard     // Walks a rectangle: along its right, then its facing.
};

UENUM(BlueprintType)
enum class EGatherAndHuntHazardParam : uint8
{
	Length,  // Patrol length / loop depth (steps along the facing).
	Width,   // Loop width (steps along the right).
	Stride,  // Cells per step.
	Phase    // Turns into the cycle the hazard starts at.
};

// One piece as the editor presents it: grid cells and facings rather than the
// board file's world units and rotators.
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntBoardEditorPiece
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	int32 PieceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	EGatherAndHuntBoardPieceKind Kind = EGatherAndHuntBoardPieceKind::Block;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	FIntVector Cell = FIntVector::ZeroValue;

	// turtlesim heading: 0 N (+Y), 1 E (+X), 2 S, 3 W.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	int32 Heading = 1;

	// turtlesim pitch: 0 down, 1 level, 2 up. Only turtles pitch.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	int32 Pitch = 1;

	// Turtles only.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	EGatherAndHuntTurtleRole Role = EGatherAndHuntTurtleRole::Player;

	// Moving hazards only.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	int32 Length = 1;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	int32 Width = 1;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	int32 Stride = 1;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|BoardEditor")
	int32 Phase = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGatherAndHuntBoardEditorChanged, bool, bBoardChanged);

// Edits the starting layout and metadata of the session's puzzle, in grid
// terms: a cursor picks a cell, the piece in it is the selection. Every edit is
// pushed to the session straight away (resetting it to turn 0), so the board
// view and the planner always show what would be saved. Replaces the legacy
// Blueprint level editor.
UCLASS(BlueprintType)
class GATHERANDHUNT_API UGatherAndHuntBoardEditor : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void BindToSession(UGatherAndHuntPuzzleSession* InSession);

	// Re-reads the session's puzzle and resets it to turn 0. Clears undo history.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SyncFromSession();

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	bool HasPuzzle() const { return Working != nullptr; }

	// Replaces the session's puzzle with an empty board.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool NewPuzzle(const FString& PuzzleName, FIntVector CellCount);

	// --- Cursor and selection ---
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	FIntVector GetCursor() const { return Cursor; }

	// Clamped to the board.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetCursor(FIntVector Cell);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void MoveCursor(FIntVector Delta);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	FIntVector GetCellCount() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	TArray<FGatherAndHuntBoardEditorPiece> GetPieces() const;

	// The piece under the cursor, if any.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	bool GetSelectedPiece(FGatherAndHuntBoardEditorPiece& OutPiece) const;

	// --- Piece edits (all act on the piece under the cursor) ---
	// Fails if the cell is taken.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool PlacePiece(EGatherAndHuntBoardPieceKind Kind);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool RemoveSelectedPiece();

	// Moves the selected piece and the cursor together. Fails off the board or
	// into an occupied cell.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool MoveSelectedPiece(FIntVector Delta);

	// Turns the selected piece a quarter right (+1) or left (-1).
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool RotateSelectedPiece(int32 Direction);

	// Tilts the selected turtle up (+1) or down (-1).
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool PitchSelectedPiece(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool SetSelectedTurtleRole(EGatherAndHuntTurtleRole Role);

	// Player -> Ally -> Opponent -> Player.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool CycleSelectedTurtleRole();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool AdjustSelectedHazard(EGatherAndHuntHazardParam Param, int32 Delta);

	// Cells a moving hazard visits over one cycle, starting where it stands on
	// turn 0. Empty for anything else.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	TArray<FIntVector> GetHazardPath(int32 PieceId) const;

	// --- Puzzle metadata ---
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	FString GetPuzzleName() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	FString GetIntroText() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	int32 GetTargetScore() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	int32 GetTargetTurnCount() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	int32 GetTargetObjectiveCount() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	int32 GetPuzzleAct() const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	int32 GetPuzzleOrder() const;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetPuzzleName(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetIntroText(const FString& Text);

	// Negative values clamp to 0 (meaning "no target").
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetTargetScore(int32 Value);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetTargetTurnCount(int32 Value);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetTargetObjectiveCount(int32 Value);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetPuzzleActAndOrder(int32 Act, int32 Order);

	// --- Puzzle rules ---
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	EGatherAndHuntPuzzleGoal GetGoal() const;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetGoal(EGatherAndHuntPuzzleGoal Goal);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	bool IsMoveAllowed(EGatherAndHuntPuzzleMoveKind MoveKind) const;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetMoveAllowed(EGatherAndHuntPuzzleMoveKind MoveKind, bool bAllowed);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	bool GetShowGround() const;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetShowGround(bool bShow);

	// 0 = no limit.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	int32 GetMaxTurtlesMovedPerTurn() const;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetMaxTurtlesMovedPerTurn(int32 Value);

	// Fails if a piece would end up off the board.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool ResizeBoard(FIntVector CellCount);

	// --- Validation ---
	// The level editor library's checks plus overlaps and a missing player turtle.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	TArray<FLevelEditorValidationIssue> GetIssues() const;

	// --- Undo ---
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool Undo();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool Redo();

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	bool CanUndo() const { return UndoStack.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	bool CanRedo() const { return RedoStack.Num() > 0; }

	// bBoardChanged is false for cursor-only changes.
	UPROPERTY(BlueprintAssignable, Category = "GatherAndHunt|BoardEditor")
	FGatherAndHuntBoardEditorChanged OnChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|BoardEditor")
	int32 MaxUndoSteps = 100;

	static EGatherAndHuntBoardPieceKind GetPieceKind(const FPieceSaveData& PieceData);
	static bool IsMovingHazard(EGatherAndHuntBoardPieceKind Kind)
	{
		return Kind == EGatherAndHuntBoardPieceKind::PatrolHazard || Kind == EGatherAndHuntBoardPieceKind::LoopHazard;
	}

private:
	float GetCellSize() const;
	FIntVector ToCell(const FVector& Position) const;
	FVector ToPosition(const FIntVector& Cell) const;
	bool IsInside(const FIntVector& Cell) const;
	FPieceSaveData* FindPieceAt(const FIntVector& Cell);
	const FPieceSaveData* FindPieceAt(const FIntVector& Cell) const;
	FGatherAndHuntBoardEditorPiece MakeView(const FPieceSaveData& PieceData) const;

	// Snapshot for undo; call before changing Working.
	void BeginEdit();
	// Pushes Working to the session and notifies.
	void CommitEdit();
	void RemoveScriptedMovesFor(int32 TurtleId);

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntPuzzleSession> Session;

	// Editable copy of the session's puzzle.
	UPROPERTY(Transient)
	TObjectPtr<ULevelSave> Working;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULevelSave>> UndoStack;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULevelSave>> RedoStack;

	FIntVector Cursor = FIntVector::ZeroValue;
};
