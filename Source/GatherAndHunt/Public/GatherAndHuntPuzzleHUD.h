#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GatherAndHuntPuzzleHUD.generated.h"

class AGatherAndHuntPuzzleBoardView;
class UGatherAndHuntPlannerWidget;
class UGatherAndHuntTurnPlanner;

// Default HUD for AGatherAndHuntPuzzleGameMode. Once the GameMode has created
// its puzzle session, wires up the turn planner, a board view (reusing one
// placed in the level if present) and the planner widget, and takes the camera.
UCLASS()
class GATHERANDHUNT_API AGatherAndHuntPuzzleHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGatherAndHuntPuzzleHUD();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	UGatherAndHuntTurnPlanner* GetPlanner() const { return Planner; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Planner")
	AGatherAndHuntPuzzleBoardView* GetBoardView() const { return BoardView; }

	// Swap in a UMG Blueprint subclass to restyle the planner.
	UPROPERTY(EditDefaultsOnly, Category = "GatherAndHunt|Planner")
	TSubclassOf<UGatherAndHuntPlannerWidget> PlannerWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GatherAndHunt|Planner")
	TSubclassOf<AGatherAndHuntPuzzleBoardView> BoardViewClass;

private:
	bool TryInitialize();

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntTurnPlanner> Planner;

	UPROPERTY(Transient)
	TObjectPtr<AGatherAndHuntPuzzleBoardView> BoardView;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntPlannerWidget> PlannerWidget;
};
