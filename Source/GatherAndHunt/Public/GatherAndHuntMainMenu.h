#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GatherAndHuntFrontEnd.h"
#include "GatherAndHuntMainMenu.generated.h"

class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UGatherAndHuntPlannerButton;

// Title screen and puzzle select: lists the board files, plays or edits the
// chosen one, or starts a new puzzle in the board editor.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGatherAndHuntMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	// Rescans Content/BoardSetups.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|FrontEnd")
	void RefreshPuzzles();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildTree();
	void RefreshDetails();
	void OnPuzzle(int32 Index);
	void OnAction(int32 Action);
	void ShowMessage(const FString& Message, bool bIsError);

	UPROPERTY(Transient) TObjectPtr<UScrollBox> PuzzleList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailText;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> PlayButton;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> EditButton;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> NewFileBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TArray<TObjectPtr<UGatherAndHuntPlannerButton>> PuzzleButtons;

	TArray<FGatherAndHuntPuzzleListing> Puzzles;
	int32 SelectedIndex = INDEX_NONE;
};

// Puts the main menu on screen for the local player.
UCLASS()
class GATHERANDHUNT_API AGatherAndHuntMenuHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGatherAndHuntMenuHUD();

	virtual void BeginPlay() override;

	// Swap in a UMG Blueprint subclass to restyle the menu.
	UPROPERTY(EditDefaultsOnly, Category = "GatherAndHunt|FrontEnd")
	TSubclassOf<UGatherAndHuntMainMenuWidget> MenuWidgetClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntMainMenuWidget> MenuWidget;
};

// GameMode for the front end: no pawn, just the menu HUD.
UCLASS()
class GATHERANDHUNT_API AGatherAndHuntMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGatherAndHuntMenuGameMode();
};
