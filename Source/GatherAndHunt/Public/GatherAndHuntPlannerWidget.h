#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GatherAndHuntCameraInput.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPlannerWidget.generated.h"

class AGatherAndHuntPuzzleBoardView;
class UBorder;
class UCheckBox;
class UComboBoxString;
class UGatherAndHuntInput;
class UGatherAndHuntPuzzleSession;
class UGatherAndHuntTurnPlanner;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWrapBox;

DECLARE_DELEGATE_OneParam(FGatherAndHuntIndexedClick, int32);

// Button that reports a payload index when clicked, so lists can share one
// handler. Never takes keyboard focus, so shortcut keys keep reaching the planner.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntPlannerButton : public UButton
{
	GENERATED_BODY()

public:
	UGatherAndHuntPlannerButton(const FObjectInitializer& ObjectInitializer);

	int32 Payload = INDEX_NONE;
	FGatherAndHuntIndexedClick OnIndexedClick;

	UFUNCTION()
	void HandleClicked();
};

// Move planner UI (spec §16) for puzzle sessions. Builds its own widget tree in
// C++, so it can be used directly or as the parent of a UMG Blueprint that
// supplies a designed tree instead.
//
// The plain screen is the picture move buttons, Undo and Execute; "Show moves"
// brings out the queued moves, the budget, the turn's numbers and the key list.
//
// Keyboard and gamepad arrive as UGatherAndHuntInput actions (BindInput); the
// keys below are that table's defaults. The mouse is handled here.
//
// Tab switches what the letter keys do. By default they fly the camera (W/S and
// A/D, Q/E down and up, Shift faster). After Tab they queue moves: W/Up forward,
// A/Left & D/Right yaw, R/F pitch up/down, S wait.
// Either way: Backspace delete, , and . shift move, [ ] or 1-9 pick ship, Esc back,
// C clear ship (Shift+C all), P toggle scripted preview, M show moves, Home
// reframe, F5 restart, Enter submit (or skip playback). Left drag orbits, right
// drag looks around, middle drag slides, wheel zooms.
// Authoring (the menu at the bottom left, when the GameMode offers it): O toggles
// editing the script (allies + opponents; Enter saves the turn), Ctrl+S writes
// the board file, B opens the board editor.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntPlannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGatherAndHuntPlannerWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void Bind(UGatherAndHuntPuzzleSession* InSession, UGatherAndHuntTurnPlanner* InPlanner, AGatherAndHuntPuzzleBoardView* InBoardView);

	// Listens to the screen's input actions. The widget doesn't read keys.
	void BindInput(UGatherAndHuntInput* InInput);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void Refresh();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void ShowMessage(const FString& Message, bool bIsError);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildTree();
	void RefreshInfo();
	void RefreshTurtleList();
	void RefreshQueue();
	void RefreshPreview();
	void RefreshPuzzleFiles();
	void RefreshAuthoringMenu();
	void RefreshDetails();
	// Whether the authoring menu and its shortcuts (O, B, Ctrl+S) are offered.
	bool CanAuthor() const;

	UTextBlock* MakeText(const FString& Text, int32 Size = 11, const FLinearColor& Color = FLinearColor::White);
	UGatherAndHuntPlannerButton* MakeButton(const FString& Label, int32 Payload, void (UGatherAndHuntPlannerWidget::*Handler)(int32), int32 FontSize = 11);
	UWidget* MakePanel(UWidget* Content);

	void HandleInputPressed(EGatherAndHuntInputAction Action, float Value);
	void OnMoveButton(int32 MoveKind);
	void OnEditButton(int32 Command);
	void OnTurtleButton(int32 TurtleId);
	void OnQueueChip(int32 MoveIndex);
	void OnFileButton(int32 Command);

	void SubmitPlan();
	void LoadSelectedPuzzle();
	void ResetPuzzle();
	void SavePuzzleFile();
	void EditBoard();
	void BackToMenu();
	FString GetLoadedPuzzleFile() const;
	// Opened from the level select map rather than the menu.
	bool IsCampaignPuzzle() const;

	UFUNCTION()
	void HandlePlannerChanged();

	UFUNCTION()
	void HandlePlaybackFinished();

	UFUNCTION()
	void HandleScriptedPreviewToggled(bool bIsChecked);

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntPuzzleSession> Session;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntTurnPlanner> Planner;

	UPROPERTY(Transient)
	TObjectPtr<AGatherAndHuntPuzzleBoardView> BoardView;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntInput> Input;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> PuzzleNameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> IntroText;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> PuzzleFileCombo;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TurtleList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> QueueHeaderText;
	UPROPERTY(Transient) TObjectPtr<UWrapBox> QueueStrip;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> BudgetBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BudgetText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PreviewText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ValidationText;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> ScriptedPreviewCheck;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> SubmitButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubmitLabel;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> RevertButton;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> SideButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SideLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UWidget> TurtlePanel;
	UPROPERTY(Transient) TObjectPtr<UWidget> QueueDetails;
	UPROPERTY(Transient) TObjectPtr<UWidget> PreviewDetails;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailsLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> KeyModeText;
	UPROPERTY(Transient) TObjectPtr<UWidget> AuthoringPanel;
	UPROPERTY(Transient) TObjectPtr<UWidget> AuthoringBody;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AuthoringToggleLabel;

	// One per move kind; payload is the kind. Hidden when the puzzle doesn't allow it.
	UPROPERTY(Transient) TArray<TObjectPtr<UGatherAndHuntPlannerButton>> MoveButtons;

	// Move chips in the queue strip, in move order (rebuilt on every refresh).
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> QueueChips;

	int32 FindChipAt(const FVector2D& ScreenPosition) const;
	int32 FindDropIndex(const FVector2D& ScreenPosition) const;

	FGatherAndHuntCameraInput CameraInput;
	// The queued moves, budget, preview numbers and key list are showing.
	bool bShowDetails = false;
	bool bMenuConfirmPending = false;
	bool bAuthoringMenuOpen = false;

	// Chip press / drag-to-reorder state. PressedChipIndex follows the dragged
	// move as it is reordered.
	int32 PressedChipIndex = INDEX_NONE;
	FVector2D PressScreenPosition = FVector2D::ZeroVector;
	bool bDraggingChip = false;
};
