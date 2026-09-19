#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPlannerWidget.generated.h"

class AGatherAndHuntPuzzleBoardView;
class UCheckBox;
class UComboBoxString;
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
// Keys: W/Up forward, A/Left & D/Right yaw, R/F pitch up/down, S wait,
// Backspace delete, [ ] shift move, Esc cursor to end, Tab/1-9 pick turtle,
// C clear turtle (Shift+C all), P toggle opponent preview, Home reframe,
// Enter submit (or skip playback). Drag with either mouse button to orbit, wheel to zoom.
// Authoring: O toggles editing the scripted opponent (Enter then saves its turn),
// Ctrl+S writes the board file.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntPlannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGatherAndHuntPlannerWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void Bind(UGatherAndHuntPuzzleSession* InSession, UGatherAndHuntTurnPlanner* InPlanner, AGatherAndHuntPuzzleBoardView* InBoardView);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Planner")
	void Refresh();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
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

	UTextBlock* MakeText(const FString& Text, int32 Size = 11, const FLinearColor& Color = FLinearColor::White);
	UGatherAndHuntPlannerButton* MakeButton(const FString& Label, int32 Payload, void (UGatherAndHuntPlannerWidget::*Handler)(int32), int32 FontSize = 11);
	UWidget* MakePanel(UWidget* Content);

	void OnMoveButton(int32 MoveKind);
	void OnEditButton(int32 Command);
	void OnTurtleButton(int32 TurtleId);
	void OnQueueChip(int32 MoveIndex);
	void OnFileButton(int32 Command);

	void SubmitPlan();
	void LoadSelectedPuzzle();
	void ResetPuzzle();
	void SavePuzzleFile();
	void ShowMessage(const FString& Message, bool bIsError);
	FString GetLoadedPuzzleFile() const;

	UFUNCTION()
	void HandlePlannerChanged();

	UFUNCTION()
	void HandlePlaybackFinished();

	UFUNCTION()
	void HandleOpponentPreviewToggled(bool bIsChecked);

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntPuzzleSession> Session;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntTurnPlanner> Planner;

	UPROPERTY(Transient)
	TObjectPtr<AGatherAndHuntPuzzleBoardView> BoardView;

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
	UPROPERTY(Transient) TObjectPtr<UCheckBox> OpponentPreviewCheck;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> SubmitButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubmitLabel;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> RevertButton;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> SideButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SideLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;

	bool bOrbiting = false;
};
