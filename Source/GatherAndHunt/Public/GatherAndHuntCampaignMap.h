#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/HUD.h"
#include "GatherAndHuntCameraInput.h"
#include "GatherAndHuntPuzzleGameMode.h"
#include "GatherAndHuntCampaignMap.generated.h"

class AGatherAndHuntPuzzleBoardView;
class UGatherAndHuntCampaign;
class UGatherAndHuntInput;
class UGatherAndHuntPlannerButton;
class UGatherAndHuntPuzzleSession;
class UGatherAndHuntTurnPlanner;
class UTextBlock;
class UVerticalBox;

// The level select map's screen: plan one sortie for the ship with the moves
// and motions earned so far, launch it, and enter the level it reaches. A
// sortie that reaches nothing puts the ship back at its start.
//
// The plain screen is the picture move buttons, Undo and Launch, with the
// levels marked I, II, III. Holding Ctrl or Alt shows their names and the list
// of levels; "Show flight plan" shows the queued moves.
//
// Keyboard and gamepad arrive as UGatherAndHuntInput actions (BindInput); the
// keys below are that table's defaults. The mouse is handled here.
//
// Tab switches what the letter keys do. By default they fly the camera (W/S and
// A/D, Q/E down and up, Shift faster). After Tab they queue the motions earned
// so far: W/Up forward, A/Left & D/Right yaw, R/F pitch up/down, S wait.
// Either way: Backspace undo, C clear, [ ] or 1-9 pick ship, M show the flight
// plan, Enter launch (or skip the flight), Home reframe, Esc main menu. Left drag orbits, right drag looks around, middle
// drag slides, wheel zooms.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntCampaignMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGatherAndHuntCampaignMapWidget(const FObjectInitializer& ObjectInitializer);

	void Bind(UGatherAndHuntCampaign* InCampaign, UGatherAndHuntPuzzleSession* InSession, UGatherAndHuntTurnPlanner* InPlanner, AGatherAndHuntPuzzleBoardView* InBoardView);

	// Listens to the screen's input actions. The widget doesn't read keys.
	void BindInput(UGatherAndHuntInput* InInput);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Campaign")
	void Refresh();

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
	void HandleInputPressed(EGatherAndHuntInputAction Action, float Value);
	void OnMoveButton(int32 MoveKind);
	void OnAction(int32 Action);
	void Launch();
	// Enters the level the finished sortie reached, or sends the ship home.
	void ResolveSortie();
	void ResetProgress();
	bool IsFlying() const;
	// Puts numerals or names on the markers, and fades the finished ones.
	void ApplyMarkerLabels(bool bRebuildBoard);

	UFUNCTION()
	void HandlePlannerChanged();

	UFUNCTION()
	void HandlePlaybackFinished();

	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntCampaign> Campaign;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPuzzleSession> Session;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntTurnPlanner> Planner;
	UPROPERTY(Transient) TObjectPtr<AGatherAndHuntPuzzleBoardView> BoardView;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntInput> Input;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> AllowanceText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> NodeList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> QueueText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MovesText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> LaunchButton;
	UPROPERTY(Transient) TObjectPtr<UWidget> LevelsPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlanToggleLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> KeyModeText;

	// One per motion; payload is the move kind. Hidden until unlocked.
	UPROPERTY(Transient) TArray<TObjectPtr<UGatherAndHuntPlannerButton>> MoveButtons;

	// Nodes in range of the current allowance; recomputed when progress changes.
	TSet<int32> ReachableNodes;

	FGatherAndHuntCameraInput CameraInput;
	// The queued moves are listed.
	bool bShowPlan = false;
	// The ShowNames action is held: markers carry names, and the list of levels is up.
	bool bShowNames = false;

	bool bSortieLaunched = false;
	bool bResetConfirmPending = false;
};

// Wires the map's session to a planner, a board view and the map widget, and
// takes the camera, as AGatherAndHuntPuzzleHUD does for a puzzle.
UCLASS()
class GATHERANDHUNT_API AGatherAndHuntCampaignHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGatherAndHuntCampaignHUD();

	virtual void Tick(float DeltaSeconds) override;

	// Swap in a UMG Blueprint subclass to restyle the map screen.
	UPROPERTY(EditDefaultsOnly, Category = "GatherAndHunt|Campaign")
	TSubclassOf<UGatherAndHuntCampaignMapWidget> MapWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GatherAndHunt|Campaign")
	TSubclassOf<AGatherAndHuntPuzzleBoardView> BoardViewClass;

private:
	bool TryInitialize();

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntTurnPlanner> Planner;

	UPROPERTY(Transient)
	TObjectPtr<AGatherAndHuntPuzzleBoardView> BoardView;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntCampaignMapWidget> MapWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntInput> Input;
};

// GameMode for the level select map. It is a puzzle GameMode whose puzzle is
// built from the campaign (map board + level markers + the player's allowance)
// instead of read from a board file.
UCLASS()
class GATHERANDHUNT_API AGatherAndHuntCampaignGameMode : public AGatherAndHuntPuzzleGameMode
{
	GENERATED_BODY()

public:
	AGatherAndHuntCampaignGameMode();

	virtual void BeginPlay() override;

	// Rebuilds the map from the campaign's current progress and restarts it.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Campaign")
	bool ReloadMap();

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Campaign")
	UGatherAndHuntCampaign* GetCampaign() const { return Campaign; }

	// Why the map isn't up, when it isn't.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Campaign")
	FString LoadError;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntCampaign> Campaign;
};
