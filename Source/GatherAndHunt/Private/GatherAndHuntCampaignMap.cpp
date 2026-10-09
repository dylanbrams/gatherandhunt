#include "GatherAndHuntCampaignMap.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GatherAndHuntCampaign.h"
#include "GatherAndHuntFrontEnd.h"
#include "GatherAndHuntInput.h"
#include "GatherAndHuntPuzzleBoardView.h"
#include "GatherAndHuntPuzzleSession.h"
#include "GatherAndHuntTurnPlanner.h"
#include "GatherAndHuntWidgetBuilder.h"

using namespace GatherAndHuntUI;

namespace
{
	enum class EMapAction : int32
	{
		Launch,
		Undo,
		Clear,
		Menu,
		ResetProgress,
		TogglePlan
	};

	const EGatherAndHuntPuzzleMoveKind MapMotions[] = {
		EGatherAndHuntPuzzleMoveKind::Forward, EGatherAndHuntPuzzleMoveKind::YawLeft, EGatherAndHuntPuzzleMoveKind::YawRight,
		EGatherAndHuntPuzzleMoveKind::PitchUp, EGatherAndHuntPuzzleMoveKind::PitchDown, EGatherAndHuntPuzzleMoveKind::Wait};
}

// --- Widget -----------------------------------------------------------------------

UGatherAndHuntCampaignMapWidget::UGatherAndHuntCampaignMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UGatherAndHuntCampaignMapWidget::RebuildWidget()
{
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UGatherAndHuntCampaignMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Bind runs before the tree exists; fill it in once it's on screen.
	Refresh();
}

void UGatherAndHuntCampaignMapWidget::Bind(
	UGatherAndHuntCampaign* InCampaign,
	UGatherAndHuntPuzzleSession* InSession,
	UGatherAndHuntTurnPlanner* InPlanner,
	AGatherAndHuntPuzzleBoardView* InBoardView)
{
	Campaign = InCampaign;
	Session = InSession;
	Planner = InPlanner;
	BoardView = InBoardView;

	if (Planner != nullptr)
	{
		Planner->OnPlanChanged.AddUniqueDynamic(this, &UGatherAndHuntCampaignMapWidget::HandlePlannerChanged);
	}
	if (BoardView != nullptr)
	{
		BoardView->OnPlaybackFinished.AddUniqueDynamic(this, &UGatherAndHuntCampaignMapWidget::HandlePlaybackFinished);
	}

	ReachableNodes = Campaign != nullptr ? Campaign->FindReachableNodes() : TSet<int32>();
	Refresh();
}

void UGatherAndHuntCampaignMapWidget::BuildTree()
{
	// Transparent full-screen backdrop: catches mouse drags and the wheel on
	// empty space for the orbit camera. Panels sit on a canvas on top of it.
	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(FLinearColor::Transparent);
	Backdrop->SetPadding(FMargin(0.0f));
	Backdrop->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Backdrop;

	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Backdrop->SetContent(Canvas);

	auto Action = [this](const FString& Label, const EMapAction InAction, const int32 FontSize = 11)
	{
		return MakeButton(WidgetTree, this, Label, static_cast<int32>(InAction), &UGatherAndHuntCampaignMapWidget::OnAction, FontSize);
	};

	// --- Top left: what the ship can do ---------------------------------------
	{
		UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Box, MakeText(WidgetTree, TEXT("LEVEL SELECT"), 16), 0.0f);
		AllowanceText = MakeText(WidgetTree, TEXT(""), 12);
		AllowanceText->SetAutoWrapText(true);
		AddRow(Box, SizeTo(WidgetTree, AllowanceText, 420.0f), 4.0f);
		UTextBlock* const Hint = MakeText(WidgetTree,
			TEXT("Fly the ship onto a level to play it. Finishing a level earns more moves and motions, which bring further levels into reach. Hold Ctrl or Alt for the levels' names."),
			10, DimTextColor);
		Hint->SetAutoWrapText(true);
		AddRow(Box, SizeTo(WidgetTree, Hint, 420.0f), 4.0f);

		UHorizontalBox* const Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Buttons, Action(TEXT("Main menu (Esc)"), EMapAction::Menu));
		AddCell(Buttons, Action(TEXT("Reset progress"), EMapAction::ResetProgress));
		AddRow(Box, Buttons, 8.0f);

		MessageText = MakeText(WidgetTree, TEXT(""), 10, GoodColor);
		MessageText->SetAutoWrapText(true);
		AddRow(Box, SizeTo(WidgetTree, MessageText, 420.0f), 4.0f);
		Place(Canvas, MakePanel(WidgetTree, Box), FVector2D(0.0f, 0.0f), FVector2D(16.0f, 16.0f));
	}

	// --- Right: the levels ----------------------------------------------------
	{
		UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Box, MakeText(WidgetTree, TEXT("LEVELS"), 10, DimTextColor), 0.0f);
		NodeList = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Box, NodeList, 4.0f);
		LevelsPanel = MakePanel(WidgetTree, Box);
		Place(Canvas, LevelsPanel, FVector2D(1.0f, 0.0f), FVector2D(-16.0f, 16.0f));
	}

	// --- Bottom: the sortie -----------------------------------------------------
	{
		UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
		MovesText = MakeText(WidgetTree, TEXT(""), 12);
		AddRow(Box, MovesText, 0.0f);

		// The prototype's big picture buttons, one per motion earned.
		UHorizontalBox* const Moves = WidgetTree->ConstructWidget<UHorizontalBox>();
		MoveButtons.Reset();
		for (const EGatherAndHuntPuzzleMoveKind Motion : MapMotions)
		{
			UGatherAndHuntPlannerButton* const Button = MakeMoveButton(WidgetTree, this, Motion, &UGatherAndHuntCampaignMapWidget::OnMoveButton);
			MoveButtons.Add(Button);
			AddCell(Moves, Button);
		}
		AddCell(Moves, MakeText(WidgetTree, TEXT("   ")));
		AddCell(Moves, Action(TEXT("  Undo  "), EMapAction::Undo, 13));
		AddCell(Moves, Action(TEXT("  Clear  "), EMapAction::Clear, 13), 16.0f);
		LaunchButton = Action(TEXT("  LAUNCH  "), EMapAction::Launch, 18);
		AddCell(Moves, LaunchButton, 16.0f);
		UGatherAndHuntPlannerButton* const PlanToggle = Action(TEXT(""), EMapAction::TogglePlan);
		PlanToggleLabel = Cast<UTextBlock>(PlanToggle->GetContent());
		AddCell(Moves, PlanToggle);
		AddRow(Box, Moves, 6.0f);

		KeyModeText = MakeText(WidgetTree, TEXT(""), 10, DimTextColor);
		AddRow(Box, KeyModeText, 6.0f);
		QueueText = MakeText(WidgetTree, TEXT(""), 11);
		QueueText->SetAutoWrapText(true);
		AddRow(Box, SizeTo(WidgetTree, QueueText, 760.0f), 4.0f);
		Place(Canvas, MakePanel(WidgetTree, Box), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -16.0f));
	}
}

void UGatherAndHuntCampaignMapWidget::Refresh()
{
	if (AllowanceText == nullptr)
	{
		return; // Tree not built (or supplied by a Blueprint subclass).
	}

	NodeList->ClearChildren();
	const bool bHasMap = Campaign != nullptr && Session != nullptr && Session->IsPuzzleLoaded() && Planner != nullptr;
	for (UGatherAndHuntPlannerButton* const Button : MoveButtons)
	{
		const bool bUnlocked = bHasMap && Planner->IsMoveAvailable(static_cast<EGatherAndHuntPuzzleMoveKind>(Button->Payload));
		Button->SetVisibility(bUnlocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!bHasMap)
	{
		const AGatherAndHuntCampaignGameMode* const GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AGatherAndHuntCampaignGameMode>() : nullptr;
		AllowanceText->SetText(FText::FromString(GameMode != nullptr && !GameMode->LoadError.IsEmpty()
			? GameMode->LoadError
			: FString(TEXT("The level select map isn't loaded."))));
		AllowanceText->SetColorAndOpacity(FSlateColor(ErrorColor));
		MovesText->SetText(FText::GetEmpty());
		QueueText->SetText(FText::GetEmpty());
		KeyModeText->SetText(FText::GetEmpty());
		LevelsPanel->SetVisibility(ESlateVisibility::Collapsed);
		LaunchButton->SetIsEnabled(false);
		return;
	}

	const FGatherAndHuntCampaignAllowance Allowance = Campaign->GetAllowance();
	AllowanceText->SetText(FText::FromString(FString::Printf(TEXT("Moves per flight: %d\nMotions: %s"),
		Allowance.Moves, *UGatherAndHuntCampaign::DescribeMotions(Allowance.MoveMask))));
	AllowanceText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	const TArray<FGatherAndHuntCampaignNode>& Nodes = Campaign->GetNodes();
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FGatherAndHuntCampaignNode& Node = Nodes[Index];
		const bool bDone = Campaign->IsNodeCompleted(Index);
		const bool bInRange = ReachableNodes.Contains(Index);
		FString Line = FString::Printf(TEXT("%s  %s  (%d,%d,%d)  %s"),
			*UGatherAndHuntCampaign::RomanNumeral(Index), *Campaign->GetNodeName(Index), Node.Cell.X, Node.Cell.Y, Node.Cell.Z,
			bDone ? TEXT("done") : bInRange ? TEXT("in reach") : TEXT("too far"));
		const FString Reward = UGatherAndHuntCampaign::DescribeReward(Node.Reward);
		if (!bDone && !Reward.IsEmpty())
		{
			Line += FString::Printf(TEXT("\n     earns %s"), *Reward);
		}
		NodeList->AddChildToVerticalBox(MakeText(WidgetTree, Line, 10, bDone ? GoodColor : bInRange ? FLinearColor::White : DimTextColor))
			->SetPadding(FMargin(0.0f, 2.0f));
	}
	if (Nodes.Num() == 0)
	{
		NodeList->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(TEXT("No levels in %s yet."), UGatherAndHuntCampaign::DefinitionFile), 10, DimTextColor));
	}

	int32 MovesUsed = 0;
	for (const FGatherAndHuntPlannerTurtleSummary& Summary : Planner->GetTurtleSummaries())
	{
		MovesUsed += Summary.MoveCount;
	}
	const FGatherAndHuntPuzzlePlanValidation Validation = Planner->GetValidation();
	FString MovesLine = FString::Printf(TEXT("%d / %d moves"), MovesUsed, Allowance.Moves);
	if (!Validation.bIsValid && MovesUsed > 0)
	{
		MovesLine += TEXT("   ") + Validation.ErrorMessage;
	}
	MovesText->SetText(FText::FromString(MovesLine));
	MovesText->SetColorAndOpacity(FSlateColor(Validation.bIsValid || MovesUsed == 0 ? FLinearColor::White : ErrorColor));

	FString Queue;
	const TArray<EGatherAndHuntPuzzleMoveKind> Moves = Planner->GetTurtleMoves(Planner->GetSelectedTurtleId());
	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		Queue += FString::Printf(TEXT("%d %s    "), Index + 1, UGatherAndHuntCampaign::MotionLabel(Moves[Index]));
	}
	QueueText->SetText(FText::FromString(IsFlying() ? FString(TEXT("Flying... (Enter to skip)"))
		: Moves.Num() > 0 ? Queue
		: FString(TEXT("No moves queued. Use the buttons or W/A/D/R/F/S."))));
	QueueText->SetColorAndOpacity(FSlateColor(Moves.Num() > 0 && !IsFlying() ? FLinearColor::White : DimTextColor));
	QueueText->SetVisibility(bShowPlan ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	PlanToggleLabel->SetText(FText::FromString(bShowPlan ? TEXT("Hide flight plan") : TEXT("Show flight plan")));
	KeyModeText->SetText(FText::FromString(Input != nullptr && Input->DoKeysMoveShip()
		? TEXT("Keys move the ship: W forward, A/D turn, R/F nose up/down, S wait.   Tab: keys fly the camera")
		: TEXT("Keys fly the camera: W/A/S/D, Q/E down/up. Drag to turn.   Tab: keys move the ship")));
	LevelsPanel->SetVisibility(bShowNames ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	LaunchButton->SetIsEnabled(MovesUsed > 0 && Planner->CanSubmit() && !IsFlying() && !bSortieLaunched);
}

void UGatherAndHuntCampaignMapWidget::ShowMessage(const FString& Message, const bool bIsError)
{
	if (MessageText != nullptr)
	{
		MessageText->SetText(FText::FromString(Message));
		MessageText->SetColorAndOpacity(FSlateColor(bIsError ? ErrorColor : GoodColor));
	}
}

bool UGatherAndHuntCampaignMapWidget::IsFlying() const
{
	return BoardView != nullptr && BoardView->IsPlayingBack();
}

// --- Commands -------------------------------------------------------------------

void UGatherAndHuntCampaignMapWidget::OnMoveButton(const int32 MoveKind)
{
	if (Planner != nullptr && !IsFlying() && !bSortieLaunched)
	{
		Planner->AddMove(static_cast<EGatherAndHuntPuzzleMoveKind>(MoveKind));
	}
}

void UGatherAndHuntCampaignMapWidget::OnAction(const int32 InAction)
{
	const EMapAction MapAction = static_cast<EMapAction>(InAction);
	if (MapAction != EMapAction::ResetProgress)
	{
		bResetConfirmPending = false;
	}
	switch (MapAction)
	{
	case EMapAction::Launch:
		Launch();
		break;
	case EMapAction::Undo:
		if (Planner != nullptr && !IsFlying()) { Planner->RemoveSelectedMove(); }
		break;
	case EMapAction::Clear:
		if (Planner != nullptr && !IsFlying()) { Planner->ClearAll(); }
		break;
	case EMapAction::Menu:
		UGatherAndHuntFrontEnd::OpenMainMenu(this);
		break;
	case EMapAction::ResetProgress:
		ResetProgress();
		break;
	case EMapAction::TogglePlan:
		bShowPlan = !bShowPlan;
		Refresh();
		break;
	}
}

void UGatherAndHuntCampaignMapWidget::Launch()
{
	if (Planner == nullptr || Campaign == nullptr || IsFlying() || bSortieLaunched || LaunchButton == nullptr || !LaunchButton->GetIsEnabled())
	{
		return;
	}

	// Success starts the flight through the planner's events; the board view
	// reports back through HandlePlaybackFinished.
	FGatherAndHuntPuzzlePlanValidation Result;
	bSortieLaunched = true;
	if (!Planner->Submit(Result))
	{
		bSortieLaunched = false;
		ShowMessage(Result.ErrorMessage, true);
		Refresh();
		return;
	}
	ShowMessage(FString(), false);
	if (bSortieLaunched && !IsFlying())
	{
		ResolveSortie(); // Nothing to watch (no board view).
	}
	else
	{
		Refresh();
	}
}

void UGatherAndHuntCampaignMapWidget::ResolveSortie()
{
	bSortieLaunched = false;
	if (Campaign == nullptr || Session == nullptr)
	{
		return;
	}

	const int32 Entered = Campaign->FindEnteredNode(Session->GetLastTurnResultView());
	if (Entered != INDEX_NONE)
	{
		const FGatherAndHuntCampaignNode& Node = Campaign->GetNodes()[Entered];
		UGatherAndHuntFrontEnd::OpenCampaignPuzzle(this, Node.Puzzle, Node.Id);
		return;
	}

	// Every sortie starts from the ship's starting cell.
	const bool bShipLost = Session->GetOutcome() == EGatherAndHuntPuzzleOutcome::Defeat;
	if (AGatherAndHuntPuzzleGameMode* const GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>() : nullptr)
	{
		GameMode->ResetPuzzle();
	}
	Planner->ClearAll();
	Planner->RefreshFromSession();
	if (BoardView != nullptr)
	{
		BoardView->RebuildBoard(false);
	}
	ShowMessage(bShipLost ? TEXT("The ship was lost. Back to the start.") : TEXT("That flight didn't reach a level. Back to the start."), true);
	Refresh();
}

void UGatherAndHuntCampaignMapWidget::ResetProgress()
{
	AGatherAndHuntCampaignGameMode* const GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AGatherAndHuntCampaignGameMode>() : nullptr;
	if (Campaign == nullptr || GameMode == nullptr || IsFlying() || bSortieLaunched)
	{
		return;
	}
	if (!bResetConfirmPending)
	{
		bResetConfirmPending = true;
		ShowMessage(TEXT("This forgets every finished level. Press Reset progress again to do it."), true);
		return;
	}
	bResetConfirmPending = false;

	Campaign->ResetProgress();
	GameMode->ReloadMap();
	Planner->BindToSession(Session);
	ApplyMarkerLabels(true);
	ReachableNodes = Campaign->FindReachableNodes();
	ShowMessage(TEXT("Progress reset."), false);
	Refresh();
}

void UGatherAndHuntCampaignMapWidget::ApplyMarkerLabels(const bool bRebuildBoard)
{
	if (Campaign == nullptr || BoardView == nullptr)
	{
		return;
	}
	BoardView->ObjectiveLabels = Campaign->GetMarkerLabels(bShowNames);
	BoardView->FadedObjectiveIds = Campaign->GetCompletedMarkerIds();
	if (bRebuildBoard)
	{
		BoardView->RebuildBoard(false);
	}
	else
	{
		BoardView->RefreshObjectiveLabels();
	}
}

// --- Events ---------------------------------------------------------------------

void UGatherAndHuntCampaignMapWidget::HandlePlannerChanged()
{
	Refresh();
}

void UGatherAndHuntCampaignMapWidget::HandlePlaybackFinished()
{
	if (bSortieLaunched)
	{
		ResolveSortie();
	}
	else
	{
		Refresh();
	}
}

// --- Input ----------------------------------------------------------------------

void UGatherAndHuntCampaignMapWidget::BindInput(UGatherAndHuntInput* InInput)
{
	Input = InInput;
	if (Input != nullptr)
	{
		Input->OnPressed.AddUObject(this, &UGatherAndHuntCampaignMapWidget::HandleInputPressed);
	}
	Refresh();
}

void UGatherAndHuntCampaignMapWidget::HandleInputPressed(const EGatherAndHuntInputAction Action, const float Value)
{
	switch (Action)
	{
	case EGatherAndHuntInputAction::Back: OnAction(static_cast<int32>(EMapAction::Menu)); return;
	case EGatherAndHuntInputAction::Reframe:
		if (BoardView != nullptr)
		{
			BoardView->FrameBoard();
		}
		return;
	case EGatherAndHuntInputAction::ToggleDetails: OnAction(static_cast<int32>(EMapAction::TogglePlan)); return;
	case EGatherAndHuntInputAction::ToggleKeyMode:
		// The letter keys either fly the camera or queue moves.
		if (Input != nullptr)
		{
			Input->SetKeysMoveShip(!Input->DoKeysMoveShip());
		}
		Refresh();
		return;
	default: break;
	}
	if (Planner == nullptr)
	{
		return;
	}

	if (Action == EGatherAndHuntInputAction::Submit || Action == EGatherAndHuntInputAction::SkipPlayback)
	{
		if (IsFlying())
		{
			BoardView->SkipPlayback();
		}
		else if (Action == EGatherAndHuntInputAction::Submit)
		{
			OnAction(static_cast<int32>(EMapAction::Launch));
		}
		return;
	}
	// The plan can't change once the ship is away.
	if (IsFlying() || bSortieLaunched)
	{
		return;
	}
	switch (Action)
	{
	case EGatherAndHuntInputAction::MoveForward: OnMoveButton(static_cast<int32>(EGatherAndHuntPuzzleMoveKind::Forward)); break;
	case EGatherAndHuntInputAction::MoveYawLeft: OnMoveButton(static_cast<int32>(EGatherAndHuntPuzzleMoveKind::YawLeft)); break;
	case EGatherAndHuntInputAction::MoveYawRight: OnMoveButton(static_cast<int32>(EGatherAndHuntPuzzleMoveKind::YawRight)); break;
	case EGatherAndHuntInputAction::MovePitchUp: OnMoveButton(static_cast<int32>(EGatherAndHuntPuzzleMoveKind::PitchUp)); break;
	case EGatherAndHuntInputAction::MovePitchDown: OnMoveButton(static_cast<int32>(EGatherAndHuntPuzzleMoveKind::PitchDown)); break;
	case EGatherAndHuntInputAction::MoveWait: OnMoveButton(static_cast<int32>(EGatherAndHuntPuzzleMoveKind::Wait)); break;
	case EGatherAndHuntInputAction::Undo: OnAction(static_cast<int32>(EMapAction::Undo)); break;
	case EGatherAndHuntInputAction::ClearShip:
	case EGatherAndHuntInputAction::ClearAll: OnAction(static_cast<int32>(EMapAction::Clear)); break;
	case EGatherAndHuntInputAction::NextShip: Planner->SelectAdjacentTurtle(1); break;
	case EGatherAndHuntInputAction::PreviousShip: Planner->SelectAdjacentTurtle(-1); break;
	case EGatherAndHuntInputAction::SelectShip:
	{
		const TArray<FGatherAndHuntPlannerTurtleSummary> Summaries = Planner->GetTurtleSummaries();
		const int32 Row = FMath::RoundToInt(Value) - 1;
		if (Summaries.IsValidIndex(Row))
		{
			Planner->SelectTurtle(Summaries[Row].TurtleId);
		}
		break;
	}
	default: break;
	}
}

void UGatherAndHuntCampaignMapWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	FGatherAndHuntCameraInput::Tick(BoardView, Input, InDeltaTime);

	// Names for as long as the action is held (Ctrl or Alt; View on a gamepad).
	const bool bWantNames = Input != nullptr && Input->IsHeld(EGatherAndHuntInputAction::ShowNames);
	if (bWantNames != bShowNames)
	{
		bShowNames = bWantNames;
		ApplyMarkerLabels(false);
		if (LevelsPanel != nullptr)
		{
			LevelsPanel->SetVisibility(bShowNames && Campaign != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

FReply UGatherAndHuntCampaignMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// The keyboard stays with the game, where the input actions are.
	CameraInput.BeginDrag(InMouseEvent);
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UGatherAndHuntCampaignMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CameraInput.IsDragging())
	{
		CameraInput.EndDrag();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UGatherAndHuntCampaignMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CameraInput.HandleMouseMove(BoardView, InMouseEvent))
	{
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UGatherAndHuntCampaignMapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (BoardView != nullptr)
	{
		BoardView->ZoomCamera(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

// --- HUD ------------------------------------------------------------------------

AGatherAndHuntCampaignHUD::AGatherAndHuntCampaignHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	MapWidgetClass = UGatherAndHuntCampaignMapWidget::StaticClass();
	BoardViewClass = AGatherAndHuntPuzzleBoardView::StaticClass();
}

void AGatherAndHuntCampaignHUD::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The GameMode builds the map in its BeginPlay, whose order relative to
	// ours isn't guaranteed, so poll until it has and then stop ticking.
	if (TryInitialize())
	{
		SetActorTickEnabled(false);
	}
}

bool AGatherAndHuntCampaignHUD::TryInitialize()
{
	UWorld* const World = GetWorld();
	APlayerController* const PC = GetOwningPlayerController();
	const AGatherAndHuntCampaignGameMode* const GameMode = World != nullptr ? World->GetAuthGameMode<AGatherAndHuntCampaignGameMode>() : nullptr;
	UGatherAndHuntPuzzleSession* const Session = GameMode != nullptr ? GameMode->GetPuzzleSession() : nullptr;
	if (Session == nullptr || PC == nullptr)
	{
		return false;
	}
	UGatherAndHuntCampaign* const Campaign = GameMode->GetCampaign();

	Planner = NewObject<UGatherAndHuntTurnPlanner>(this);
	// The map counts moves instead of charging the puzzle cost tiers.
	Planner->Budget = Session->GetMoveBudget();
	Planner->BindToSession(Session);

	FActorSpawnParameters Params;
	Params.Owner = this;
	BoardView = World->SpawnActor<AGatherAndHuntPuzzleBoardView>(
		BoardViewClass != nullptr ? BoardViewClass.Get() : AGatherAndHuntPuzzleBoardView::StaticClass(),
		FTransform::Identity,
		Params);
	if (Campaign != nullptr)
	{
		BoardView->ObjectiveLabels = Campaign->GetMarkerLabels(false);
		BoardView->FadedObjectiveIds = Campaign->GetCompletedMarkerIds();
	}
	BoardView->Bind(Session, Planner);

	MapWidget = CreateWidget<UGatherAndHuntCampaignMapWidget>(
		PC,
		MapWidgetClass != nullptr ? MapWidgetClass.Get() : UGatherAndHuntCampaignMapWidget::StaticClass());
	MapWidget->Bind(Campaign, Session, Planner, BoardView);
	Input = NewObject<UGatherAndHuntInput>(this);
	Input->Setup(PC);
	MapWidget->BindInput(Input);
	MapWidget->AddToViewport();

	// The board view owns the camera; keep possession from stealing it back.
	PC->bAutoManageActiveCameraTarget = false;
	PC->SetViewTarget(BoardView);
	PC->SetShowMouseCursor(true);

	// The keyboard and gamepad go to the game, where the input actions are;
	// the widget only takes the mouse.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
	return true;
}

// --- GameMode -------------------------------------------------------------------

AGatherAndHuntCampaignGameMode::AGatherAndHuntCampaignGameMode()
{
	HUDClass = AGatherAndHuntCampaignHUD::StaticClass();
	// The map is built from the campaign, not read from a board file.
	bAutoLoadPuzzleOnBeginPlay = false;
}

void AGatherAndHuntCampaignGameMode::BeginPlay()
{
	Super::BeginPlay();

	Campaign = UGatherAndHuntCampaign::Load(this, LoadError);
	ReloadMap();
}

bool AGatherAndHuntCampaignGameMode::ReloadMap()
{
	if (Campaign == nullptr)
	{
		return false;
	}

	FString Error;
	ULevelSave* const Map = Campaign->BuildMapLevelSave(this, Error);
	if (Map == nullptr || !LoadPuzzleFromLevelSave(Map))
	{
		LoadError = Map == nullptr ? Error : FString(TEXT("The map board doesn't convert to a playable board."));
		return false;
	}
	GetPuzzleSession()->SetMaxMovesPerTurn(FMath::Max(1, Campaign->GetAllowance().Moves));
	LoadError.Reset();
	return true;
}
