#include "GatherAndHuntPuzzleHUD.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GatherAndHuntPlannerWidget.h"
#include "GatherAndHuntPuzzleBlueprintLibrary.h"
#include "GatherAndHuntPuzzleBoardView.h"
#include "GatherAndHuntTurnPlanner.h"

AGatherAndHuntPuzzleHUD::AGatherAndHuntPuzzleHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PlannerWidgetClass = UGatherAndHuntPlannerWidget::StaticClass();
	BoardViewClass = AGatherAndHuntPuzzleBoardView::StaticClass();
}

void AGatherAndHuntPuzzleHUD::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The GameMode creates its session in BeginPlay, whose order relative to
	// ours isn't guaranteed, so poll until it exists and then stop ticking.
	if (TryInitialize())
	{
		SetActorTickEnabled(false);
	}
}

bool AGatherAndHuntPuzzleHUD::TryInitialize()
{
	UGatherAndHuntPuzzleSession* const Session = UGatherAndHuntPuzzleBlueprintLibrary::GetPuzzleSession(this);
	APlayerController* const PC = GetOwningPlayerController();
	UWorld* const World = GetWorld();
	if (Session == nullptr || PC == nullptr || World == nullptr)
	{
		return false;
	}

	Planner = NewObject<UGatherAndHuntTurnPlanner>(this);
	Planner->BindToSession(Session);

	for (TActorIterator<AGatherAndHuntPuzzleBoardView> It(World); It; ++It)
	{
		BoardView = *It;
		break;
	}
	if (BoardView == nullptr)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		BoardView = World->SpawnActor<AGatherAndHuntPuzzleBoardView>(
			BoardViewClass != nullptr ? BoardViewClass.Get() : AGatherAndHuntPuzzleBoardView::StaticClass(),
			FTransform::Identity,
			Params);
	}
	BoardView->Bind(Session, Planner);

	PlannerWidget = CreateWidget<UGatherAndHuntPlannerWidget>(
		PC,
		PlannerWidgetClass != nullptr ? PlannerWidgetClass.Get() : UGatherAndHuntPlannerWidget::StaticClass());
	PlannerWidget->AddToViewport();
	PlannerWidget->Bind(Session, Planner, BoardView);

	// The board view owns the camera; keep possession from stealing it back.
	PC->bAutoManageActiveCameraTarget = false;
	PC->SetViewTarget(BoardView);
	PC->SetShowMouseCursor(true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(PlannerWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	return true;
}
