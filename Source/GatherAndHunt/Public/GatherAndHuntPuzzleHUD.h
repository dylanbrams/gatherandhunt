#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GatherAndHuntPuzzleHUD.generated.h"

class AGatherAndHuntPuzzleBoardView;
class UGatherAndHuntBoardEditor;
class UGatherAndHuntBoardEditorWidget;
class UGatherAndHuntPlannerWidget;
class UGatherAndHuntTurnPlanner;
class UUserWidget;

// Default HUD for AGatherAndHuntPuzzleGameMode. Once the GameMode has created
// its puzzle session, wires up the turn planner, the board editor, a board view
// (reusing one placed in the level if present) and their widgets, and takes the
// camera. One of the two screens is up at a time: planning a turn or editing
// the board.
UCLASS()
class GATHERANDHUNT_API AGatherAndHuntPuzzleHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGatherAndHuntPuzzleHUD();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	UGatherAndHuntTurnPlanner* GetPlanner() const { return Planner; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	UGatherAndHuntBoardEditor* GetBoardEditor() const { return BoardEditor; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	AGatherAndHuntPuzzleBoardView* GetBoardView() const { return BoardView; }

	// Switches between the board editor and the planner. Editing restarts the
	// puzzle at turn 0. Refused (with a message) while the planner holds an
	// unsaved script draft.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	bool SetBoardEditing(bool bEditing);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardEditor")
	bool IsBoardEditing() const { return bBoardEditing; }

	// Swap in a UMG Blueprint subclass to restyle the planner.
	UPROPERTY(EditDefaultsOnly, Category = "GatherAndHunt|Planner")
	TSubclassOf<UGatherAndHuntPlannerWidget> PlannerWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GatherAndHunt|BoardEditor")
	TSubclassOf<UGatherAndHuntBoardEditorWidget> BoardEditorWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GatherAndHunt|Planner")
	TSubclassOf<AGatherAndHuntPuzzleBoardView> BoardViewClass;

private:
	bool TryInitialize();
	void FocusWidget(UUserWidget* Widget);

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntTurnPlanner> Planner;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntBoardEditor> BoardEditor;

	UPROPERTY(Transient)
	TObjectPtr<AGatherAndHuntPuzzleBoardView> BoardView;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntPlannerWidget> PlannerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntBoardEditorWidget> BoardEditorWidget;

	bool bBoardEditing = false;
};
