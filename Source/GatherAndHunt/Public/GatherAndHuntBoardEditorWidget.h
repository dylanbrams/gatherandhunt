#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "GatherAndHuntBoardEditorWidget.generated.h"

class AGatherAndHuntPuzzleBoardView;
class UEditableTextBox;
class UGatherAndHuntBoardEditor;
class UGatherAndHuntPlannerButton;
class UGatherAndHuntPuzzleSession;
class UMultiLineEditableTextBox;
class UTextBlock;
class UVerticalBox;

// Board editor UI: palette, selected-piece properties, puzzle metadata, board
// size, validation, save. Tree built in C++ like the planner's.
//
// Keys: W/A/S/D or arrows move the cursor (relative to the camera), Q/E change
// layer; hold Shift to carry the piece along. 1-7 place, Delete removes,
// R / Shift+R rotate, T/G pitch a turtle, C cycles a turtle's role,
// Ctrl+Z / Ctrl+Y undo / redo, Ctrl+S save, B back to the planner, Home reframe.
// Click a cell or piece to put the cursor there; drag to orbit, wheel to zoom.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntBoardEditorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGatherAndHuntBoardEditorWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void Bind(UGatherAndHuntPuzzleSession* InSession, UGatherAndHuntBoardEditor* InEditor, AGatherAndHuntPuzzleBoardView* InBoardView);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void Refresh();

	// File the Save button writes, relative to Content/BoardSetups.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void SetTargetFile(const FString& RelativeFilename);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardEditor")
	void ShowMessage(const FString& Message, bool bIsError);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildTree();
	void RefreshPuzzle();
	void RefreshSelection();
	void RefreshIssues();
	UWidget* MakeStepper(const FString& Label, int32 Field, TObjectPtr<UTextBlock>& OutValue);

	void OnPaletteButton(int32 Kind);
	void OnStepper(int32 Payload);
	void OnAction(int32 Action);
	void OnMoveToggle(int32 MoveKind);

	// Grid step for a screen direction (0 = away from the camera, 1 = right...).
	FIntVector CameraRelativeStep(int32 ScreenDirection) const;
	void Step(const FIntVector& Delta, bool bCarryPiece);
	void PickCellAt(const FVector2D& ScreenPosition);

	void Save();
	void NewPuzzle();
	void BackToPlanner();
	void BackToMenu();
	// For actions that throw away unsaved work: true on the second press.
	bool ConfirmDiscard(int32 Action, const FString& What);

	UFUNCTION()
	void HandleEditorChanged(bool bBoardChanged);

	UFUNCTION()
	void HandleNameCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleFileCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleIntroCommitted(const FText& Text, ETextCommit::Type CommitType);

	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPuzzleSession> Session;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntBoardEditor> Editor;
	UPROPERTY(Transient) TObjectPtr<AGatherAndHuntPuzzleBoardView> BoardView;

	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> FileBox;
	UPROPERTY(Transient) TObjectPtr<UMultiLineEditableTextBox> IntroBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetScoreValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetTurnsValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetObjectivesValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ActValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OrderValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SizeXValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SizeYValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SizeZValue;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> GoalButton;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> GroundButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ShipsPerTurnValue;
	// One per move kind; payload is the kind. Green when the player may use it.
	UPROPERTY(Transient) TArray<TObjectPtr<UGatherAndHuntPlannerButton>> MoveToggles;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SelectionText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TurtleControls;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> HazardControls;
	UPROPERTY(Transient) TObjectPtr<UWidget> WidthStepper;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LengthValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WidthValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StrideValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PhaseValue;
	UPROPERTY(Transient) TArray<TObjectPtr<UGatherAndHuntPlannerButton>> RoleButtons;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> IssueList;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> UndoButton;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> RedoButton;

	FString TargetFile;
	// Action waiting for a second press to confirm (see ConfirmDiscard), and
	// the file a second Save would overwrite.
	int32 PendingConfirmAction = INDEX_NONE;
	FString PendingOverwriteFile;

	bool bPointerDown = false;
	bool bOrbiting = false;
	FVector2D PressScreenPosition = FVector2D::ZeroVector;
};
