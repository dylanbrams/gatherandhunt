#include "GatherAndHuntPuzzleHUD.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GatherAndHuntBoardEditor.h"
#include "GatherAndHuntBoardEditorWidget.h"
#include "GatherAndHuntInput.h"
#include "Framework/Application/SlateApplication.h"
#include "GatherAndHuntPlannerWidget.h"
#include "GatherAndHuntPuzzleBlueprintLibrary.h"
#include "GatherAndHuntPuzzleBoardView.h"
#include "GatherAndHuntPuzzleGameMode.h"
#include "GatherAndHuntPuzzleSession.h"
#include "GatherAndHuntTurnPlanner.h"
#include "Misc/Paths.h"

namespace
{
	const FIntVector DefaultNewBoardSize(6, 6, 3);
}

AGatherAndHuntPuzzleHUD::AGatherAndHuntPuzzleHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PlannerWidgetClass = UGatherAndHuntPlannerWidget::StaticClass();
	BoardEditorWidgetClass = UGatherAndHuntBoardEditorWidget::StaticClass();
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
	BoardEditor = NewObject<UGatherAndHuntBoardEditor>(this);

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
	PlannerWidget->Bind(Session, Planner, BoardView);
	Input = NewObject<UGatherAndHuntInput>(this);
	Input->Setup(PC);
	PlannerWidget->BindInput(Input);

	BoardEditorWidget = CreateWidget<UGatherAndHuntBoardEditorWidget>(
		PC,
		BoardEditorWidgetClass != nullptr ? BoardEditorWidgetClass.Get() : UGatherAndHuntBoardEditorWidget::StaticClass());

	// The board view owns the camera; keep possession from stealing it back.
	PC->bAutoManageActiveCameraTarget = false;
	PC->SetViewTarget(BoardView);
	PC->SetShowMouseCursor(true);

	const AGatherAndHuntPuzzleGameMode* const GameMode = World->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>();
	const bool bStartEditing = GameMode != nullptr && GameMode->bStartInBoardEditor;
	if (bStartEditing && !Session->IsPuzzleLoaded())
	{
		// ?Edit with a file that doesn't exist yet: start that puzzle empty.
		const FString File = GameMode->AutoLoadPuzzleRelativeFilename;
		BoardEditor->BindToSession(Session);
		BoardEditor->NewPuzzle(File.IsEmpty() ? TEXT("New puzzle") : FPaths::GetBaseFilename(File), DefaultNewBoardSize);
	}
	if (GameMode != nullptr)
	{
		BoardEditorWidget->SetTargetFile(GameMode->AutoLoadPuzzleRelativeFilename);
	}

	PlannerWidget->AddToViewport();
	FocusGame();
	if (bStartEditing)
	{
		SetBoardEditing(true);
	}
	return true;
}

bool AGatherAndHuntPuzzleHUD::SetBoardEditing(const bool bEditing)
{
	if (PlannerWidget == nullptr || BoardEditorWidget == nullptr || bEditing == bBoardEditing)
	{
		return false;
	}
	UGatherAndHuntPuzzleSession* const Session = UGatherAndHuntPuzzleBlueprintLibrary::GetPuzzleSession(this);

	if (bEditing)
	{
		if (Planner->HasUnsavedScriptDraft())
		{
			PlannerWidget->ShowMessage(TEXT("Save or revert the script draft before editing the board."), true);
			return false;
		}
		if (BoardView->IsPlayingBack())
		{
			BoardView->SkipPlayback();
		}
		bBoardEditing = true;
		// Resets the session to turn 0; the editor always edits the starting layout.
		BoardEditor->BindToSession(Session);
		if (const AGatherAndHuntPuzzleGameMode* const GameMode = GetWorld()->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>())
		{
			BoardEditorWidget->SetTargetFile(GameMode->AutoLoadPuzzleRelativeFilename);
		}
		BoardEditorWidget->Bind(Session, BoardEditor, BoardView);
		BoardView->SetBoardEditor(BoardEditor);
		PlannerWidget->RemoveFromParent();
		BoardEditorWidget->AddToViewport();
		Input->SetEnabled(false);
		FocusWidget(BoardEditorWidget);
	}
	else
	{
		bBoardEditing = false;
		BoardView->SetBoardEditor(nullptr);
		// The board may have changed under the planner's queued moves.
		Planner->ClearAll();
		Planner->RefreshFromSession();
		BoardEditorWidget->RemoveFromParent();
		PlannerWidget->AddToViewport();
		PlannerWidget->Refresh();
		FocusGame();
	}
	return true;
}

void AGatherAndHuntPuzzleHUD::FocusGame()
{
	APlayerController* const PC = GetOwningPlayerController();
	if (PC == nullptr)
	{
		return;
	}
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
	if (Input != nullptr)
	{
		Input->SetEnabled(true);
	}
}

void AGatherAndHuntPuzzleHUD::FocusWidget(UUserWidget* Widget)
{
	APlayerController* const PC = GetOwningPlayerController();
	if (PC == nullptr || Widget == nullptr)
	{
		return;
	}
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
}
