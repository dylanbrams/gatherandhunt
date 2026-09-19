#include "GatherAndHuntPlannerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/World.h"
#include "GatherAndHuntPuzzleBoardView.h"
#include "GatherAndHuntPuzzleGameMode.h"
#include "GatherAndHuntPuzzleSession.h"
#include "GatherAndHuntTurnPlanner.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
	enum class EEditCommand : int32
	{
		ShiftLeft,
		ShiftRight,
		Delete,
		ClearTurtle,
		ClearAll,
		Submit,
		SkipPlayback,
		FrameBoard,
		ToggleSide,
		RevertOpponent
	};

	enum class EFileCommand : int32
	{
		Load,
		Reset,
		RefreshList,
		Save
	};

	const FLinearColor PanelColor(0.02f, 0.02f, 0.03f, 0.78f);
	const FLinearColor DimTextColor(0.65f, 0.65f, 0.7f);
	const FLinearColor ErrorColor(1.0f, 0.3f, 0.25f);
	const FLinearColor GoodColor(0.35f, 1.0f, 0.45f);
	const FLinearColor SelectedColor(0.0f, 0.55f, 0.75f);
	const FLinearColor TierColors[4] = {
		FLinearColor(0.2f, 0.2f, 0.22f),  // unused (tier 0)
		FLinearColor(0.2f, 0.2f, 0.22f),  // tier 1
		FLinearColor(0.55f, 0.38f, 0.05f), // tier 2
		FLinearColor(0.6f, 0.12f, 0.1f)};  // tier 3

	const TCHAR* MoveLabel(const EGatherAndHuntPuzzleMoveKind Kind)
	{
		switch (Kind)
		{
		case EGatherAndHuntPuzzleMoveKind::Forward: return TEXT("FWD");
		case EGatherAndHuntPuzzleMoveKind::YawLeft: return TEXT("YAW L");
		case EGatherAndHuntPuzzleMoveKind::YawRight: return TEXT("YAW R");
		case EGatherAndHuntPuzzleMoveKind::PitchUp: return TEXT("UP");
		case EGatherAndHuntPuzzleMoveKind::PitchDown: return TEXT("DOWN");
		case EGatherAndHuntPuzzleMoveKind::Wait: return TEXT("WAIT");
		}
		return TEXT("?");
	}

	FString FacingLabel(const int32 Heading, const int32 Pitch)
	{
		static const TCHAR* Headings[] = {TEXT("N"), TEXT("E"), TEXT("S"), TEXT("W")};
		const TCHAR* const HeadingName = Headings[FMath::Clamp(Heading, 0, 3)];
		if (Pitch == 0) return FString::Printf(TEXT("%s/down"), HeadingName);
		if (Pitch == 2) return FString::Printf(TEXT("%s/up"), HeadingName);
		return HeadingName;
	}

	const TCHAR* FateLabel(const EGatherAndHuntPlannerStepKind Fate)
	{
		switch (Fate)
		{
		case EGatherAndHuntPlannerStepKind::Destroyed: return TEXT("  DESTROYED");
		case EGatherAndHuntPlannerStepKind::Delivered: return TEXT("  delivers");
		case EGatherAndHuntPlannerStepKind::Blocked: return TEXT("  blocked");
		default: return TEXT("");
		}
	}

	const TCHAR* OutcomeLabel(const EGatherAndHuntPuzzleOutcome Outcome)
	{
		switch (Outcome)
		{
		case EGatherAndHuntPuzzleOutcome::Victory: return TEXT("VICTORY");
		case EGatherAndHuntPuzzleOutcome::Defeat: return TEXT("DEFEAT");
		case EGatherAndHuntPuzzleOutcome::InProgress: return TEXT("in progress");
		default: return TEXT("no puzzle loaded");
		}
	}

	FString BoardSetupsDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("BoardSetups"));
	}
}

UGatherAndHuntPlannerButton::UGatherAndHuntPlannerButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitIsFocusable(false);
}

void UGatherAndHuntPlannerButton::HandleClicked()
{
	OnIndexedClick.ExecuteIfBound(Payload);
}

UGatherAndHuntPlannerWidget::UGatherAndHuntPlannerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UGatherAndHuntPlannerWidget::RebuildWidget()
{
	// A UMG Blueprint subclass may supply its own tree; only build ours if empty.
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UGatherAndHuntPlannerWidget::Bind(
	UGatherAndHuntPuzzleSession* InSession,
	UGatherAndHuntTurnPlanner* InPlanner,
	AGatherAndHuntPuzzleBoardView* InBoardView)
{
	if (Planner != nullptr)
	{
		Planner->OnPlanChanged.RemoveDynamic(this, &UGatherAndHuntPlannerWidget::HandlePlannerChanged);
	}
	if (BoardView != nullptr)
	{
		BoardView->OnPlaybackFinished.RemoveDynamic(this, &UGatherAndHuntPlannerWidget::HandlePlaybackFinished);
	}

	Session = InSession;
	Planner = InPlanner;
	BoardView = InBoardView;

	if (Planner != nullptr)
	{
		Planner->OnPlanChanged.AddDynamic(this, &UGatherAndHuntPlannerWidget::HandlePlannerChanged);
	}
	if (BoardView != nullptr)
	{
		BoardView->OnPlaybackFinished.AddDynamic(this, &UGatherAndHuntPlannerWidget::HandlePlaybackFinished);
	}

	RefreshPuzzleFiles();
	Refresh();
}

// --- Tree construction ------------------------------------------------------

UTextBlock* UGatherAndHuntPlannerWidget::MakeText(const FString& Text, const int32 Size, const FLinearColor& Color)
{
	UTextBlock* const Block = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetText(FText::FromString(Text));
	return Block;
}

UGatherAndHuntPlannerButton* UGatherAndHuntPlannerWidget::MakeButton(
	const FString& Label,
	const int32 Payload,
	void (UGatherAndHuntPlannerWidget::*Handler)(int32),
	const int32 FontSize)
{
	UGatherAndHuntPlannerButton* const Button = WidgetTree->ConstructWidget<UGatherAndHuntPlannerButton>();
	Button->Payload = Payload;
	Button->OnIndexedClick.BindUObject(this, Handler);
	Button->OnClicked.AddDynamic(Button, &UGatherAndHuntPlannerButton::HandleClicked);
	Button->SetContent(MakeText(Label, FontSize));
	return Button;
}

void UGatherAndHuntPlannerWidget::ShowMessage(const FString& Message, const bool bIsError)
{
	if (MessageText != nullptr)
	{
		MessageText->SetText(FText::FromString(Message));
		MessageText->SetColorAndOpacity(FSlateColor(bIsError ? ErrorColor : GoodColor));
	}
}

FString UGatherAndHuntPlannerWidget::GetLoadedPuzzleFile() const
{
	const UWorld* const World = GetWorld();
	const AGatherAndHuntPuzzleGameMode* const GameMode = World != nullptr ? World->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>() : nullptr;
	if (GameMode == nullptr || GameMode->AutoLoadPuzzleRelativeFilename.IsEmpty())
	{
		return FString();
	}
	const FString& File = GameMode->AutoLoadPuzzleRelativeFilename;
	return File.EndsWith(TEXT(".dat")) ? File : File + TEXT(".dat");
}

UWidget* UGatherAndHuntPlannerWidget::MakePanel(UWidget* Content)
{
	UBorder* const Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(10.0f));
	Panel->SetContent(Content);
	return Panel;
}

void UGatherAndHuntPlannerWidget::BuildTree()
{
	// Transparent full-screen backdrop: catches mouse drags/wheel on empty space
	// for the orbit camera. Panels sit on a canvas on top of it.
	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(FLinearColor::Transparent);
	Backdrop->SetPadding(FMargin(0.0f));
	Backdrop->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Backdrop;

	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Backdrop->SetContent(Canvas);

	auto Place = [Canvas](UWidget* Widget, const FVector2D Anchor, const FVector2D Offset)
	{
		UCanvasPanelSlot* const CanvasSlot = Canvas->AddChildToCanvas(Widget);
		CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		CanvasSlot->SetAlignment(Anchor);
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetPosition(Offset);
	};

	auto AddRow = [](UVerticalBox* Box, UWidget* Widget, const float Top = 2.0f)
	{
		Box->AddChildToVerticalBox(Widget)->SetPadding(FMargin(0.0f, Top, 0.0f, 0.0f));
	};

	auto AddCell = [](UHorizontalBox* Box, UWidget* Widget)
	{
		UHorizontalBoxSlot* const CellSlot = Box->AddChildToHorizontalBox(Widget);
		CellSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
		CellSlot->SetVerticalAlignment(VAlign_Center);
	};

	// --- Top left: puzzle info + file controls -----------------------------
	{
		UVerticalBox* const Info = WidgetTree->ConstructWidget<UVerticalBox>();
		PuzzleNameText = MakeText(TEXT(""), 16);
		StatusText = MakeText(TEXT(""), 11, DimTextColor);
		IntroText = MakeText(TEXT(""), 10, DimTextColor);
		IntroText->SetAutoWrapText(true);
		AddRow(Info, PuzzleNameText, 0.0f);
		AddRow(Info, StatusText);

		USizeBox* const IntroSize = WidgetTree->ConstructWidget<USizeBox>();
		IntroSize->SetWidthOverride(420.0f);
		IntroSize->SetContent(IntroText);
		AddRow(Info, IntroSize, 4.0f);

		UHorizontalBox* const FileRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		PuzzleFileCombo = WidgetTree->ConstructWidget<UComboBoxString>();
		USizeBox* const ComboSize = WidgetTree->ConstructWidget<USizeBox>();
		ComboSize->SetWidthOverride(220.0f);
		ComboSize->SetContent(PuzzleFileCombo);
		AddCell(FileRow, ComboSize);
		AddCell(FileRow, MakeButton(TEXT("Load"), static_cast<int32>(EFileCommand::Load), &UGatherAndHuntPlannerWidget::OnFileButton));
		AddCell(FileRow, MakeButton(TEXT("Reset"), static_cast<int32>(EFileCommand::Reset), &UGatherAndHuntPlannerWidget::OnFileButton));
		AddCell(FileRow, MakeButton(TEXT("Rescan"), static_cast<int32>(EFileCommand::RefreshList), &UGatherAndHuntPlannerWidget::OnFileButton));
		AddCell(FileRow, MakeButton(TEXT("Save file (Ctrl+S)"), static_cast<int32>(EFileCommand::Save), &UGatherAndHuntPlannerWidget::OnFileButton));
		AddRow(Info, FileRow, 8.0f);

		MessageText = MakeText(TEXT(""), 10, GoodColor);
		AddRow(Info, MessageText, 4.0f);

		Place(MakePanel(Info), FVector2D(0.0f, 0.0f), FVector2D(16.0f, 16.0f));
	}

	// --- Left: turtle list --------------------------------------------------
	{
		UVerticalBox* const Turtles = WidgetTree->ConstructWidget<UVerticalBox>();
		SideButton = MakeButton(TEXT(""), static_cast<int32>(EEditCommand::ToggleSide), &UGatherAndHuntPlannerWidget::OnEditButton, 10);
		SideLabel = Cast<UTextBlock>(SideButton->GetContent());
		AddRow(Turtles, SideButton, 0.0f);
		AddRow(Turtles, MakeText(TEXT("TURTLES  (Tab / 1-9)"), 10, DimTextColor), 6.0f);
		TurtleList = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Turtles, TurtleList, 4.0f);
		Place(MakePanel(Turtles), FVector2D(0.0f, 0.5f), FVector2D(16.0f, 0.0f));
	}

	// --- Right: preview, validation, submit --------------------------------
	{
		UVerticalBox* const Preview = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Preview, MakeText(TEXT("PREVIEW"), 10, DimTextColor), 0.0f);
		PreviewText = MakeText(TEXT(""), 11);
		AddRow(Preview, PreviewText);

		UHorizontalBox* const ToggleRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		OpponentPreviewCheck = WidgetTree->ConstructWidget<UCheckBox>();
		OpponentPreviewCheck->OnCheckStateChanged.AddDynamic(this, &UGatherAndHuntPlannerWidget::HandleOpponentPreviewToggled);
		AddCell(ToggleRow, OpponentPreviewCheck);
		AddCell(ToggleRow, MakeText(TEXT("Include scripted opponent (P)"), 10, DimTextColor));
		AddRow(Preview, ToggleRow, 6.0f);

		ValidationText = MakeText(TEXT(""), 11, ErrorColor);
		ValidationText->SetAutoWrapText(true);
		USizeBox* const ValidationSize = WidgetTree->ConstructWidget<USizeBox>();
		ValidationSize->SetWidthOverride(280.0f);
		ValidationSize->SetContent(ValidationText);
		AddRow(Preview, ValidationSize, 6.0f);

		UHorizontalBox* const SubmitRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		SubmitButton = MakeButton(TEXT(""), static_cast<int32>(EEditCommand::Submit), &UGatherAndHuntPlannerWidget::OnEditButton, 13);
		SubmitLabel = Cast<UTextBlock>(SubmitButton->GetContent());
		AddCell(SubmitRow, SubmitButton);
		AddCell(SubmitRow, MakeButton(TEXT("Skip anim"), static_cast<int32>(EEditCommand::SkipPlayback), &UGatherAndHuntPlannerWidget::OnEditButton));
		RevertButton = MakeButton(TEXT("Revert"), static_cast<int32>(EEditCommand::RevertOpponent), &UGatherAndHuntPlannerWidget::OnEditButton);
		AddCell(SubmitRow, RevertButton);
		AddRow(Preview, SubmitRow, 8.0f);

		UTextBlock* const Help = MakeText(
			TEXT("W fwd   A/D yaw   R/F pitch   S wait\n")
			TEXT("Bksp delete   [ ] shift   Esc cursor to end\n")
			TEXT("C clear turtle   Shift+C clear all\n")
			TEXT("O edit scripted opponent   Ctrl+S save file\n")
			TEXT("Drag: orbit   Wheel: zoom   Home: reframe"),
			9, DimTextColor);
		AddRow(Preview, Help, 10.0f);

		Place(MakePanel(Preview), FVector2D(1.0f, 0.0f), FVector2D(-16.0f, 16.0f));
	}

	// --- Bottom: move queue, move buttons, budget ---------------------------
	{
		UVerticalBox* const Queue = WidgetTree->ConstructWidget<UVerticalBox>();
		QueueHeaderText = MakeText(TEXT(""), 11);
		AddRow(Queue, QueueHeaderText, 0.0f);

		QueueStrip = WidgetTree->ConstructWidget<UWrapBox>();
		QueueStrip->SetInnerSlotPadding(FVector2D(3.0f, 3.0f));
		QueueStrip->SetExplicitWrapSize(true);
		QueueStrip->SetWrapSize(760.0f);
		USizeBox* const StripSize = WidgetTree->ConstructWidget<USizeBox>();
		StripSize->SetMinDesiredHeight(34.0f);
		StripSize->SetWidthOverride(760.0f);
		StripSize->SetContent(QueueStrip);
		AddRow(Queue, StripSize, 4.0f);

		UHorizontalBox* const Moves = WidgetTree->ConstructWidget<UHorizontalBox>();
		const EGatherAndHuntPuzzleMoveKind MoveKinds[] = {
			EGatherAndHuntPuzzleMoveKind::Forward, EGatherAndHuntPuzzleMoveKind::YawLeft, EGatherAndHuntPuzzleMoveKind::YawRight,
			EGatherAndHuntPuzzleMoveKind::PitchUp, EGatherAndHuntPuzzleMoveKind::PitchDown, EGatherAndHuntPuzzleMoveKind::Wait};
		for (const EGatherAndHuntPuzzleMoveKind Kind : MoveKinds)
		{
			AddCell(Moves, MakeButton(FString::Printf(TEXT(" %s "), MoveLabel(Kind)), static_cast<int32>(Kind), &UGatherAndHuntPlannerWidget::OnMoveButton, 12));
		}
		AddCell(Moves, MakeText(TEXT("   "), 11));
		AddCell(Moves, MakeButton(TEXT(" < "), static_cast<int32>(EEditCommand::ShiftLeft), &UGatherAndHuntPlannerWidget::OnEditButton));
		AddCell(Moves, MakeButton(TEXT(" > "), static_cast<int32>(EEditCommand::ShiftRight), &UGatherAndHuntPlannerWidget::OnEditButton));
		AddCell(Moves, MakeButton(TEXT("Delete"), static_cast<int32>(EEditCommand::Delete), &UGatherAndHuntPlannerWidget::OnEditButton));
		AddCell(Moves, MakeButton(TEXT("Clear"), static_cast<int32>(EEditCommand::ClearTurtle), &UGatherAndHuntPlannerWidget::OnEditButton));
		AddCell(Moves, MakeButton(TEXT("Clear all"), static_cast<int32>(EEditCommand::ClearAll), &UGatherAndHuntPlannerWidget::OnEditButton));
		AddRow(Queue, Moves, 6.0f);

		UHorizontalBox* const BudgetRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		BudgetBar = WidgetTree->ConstructWidget<UProgressBar>();
		USizeBox* const BarSize = WidgetTree->ConstructWidget<USizeBox>();
		BarSize->SetWidthOverride(420.0f);
		BarSize->SetHeightOverride(14.0f);
		BarSize->SetContent(BudgetBar);
		AddCell(BudgetRow, BarSize);
		BudgetText = MakeText(TEXT(""), 11);
		AddCell(BudgetRow, BudgetText);
		AddRow(Queue, BudgetRow, 8.0f);

		Place(MakePanel(Queue), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -16.0f));
	}
}

// --- Refresh ------------------------------------------------------------------

void UGatherAndHuntPlannerWidget::Refresh()
{
	if (PuzzleNameText == nullptr)
	{
		return; // Tree not built (or supplied by a Blueprint subclass).
	}
	RefreshInfo();
	RefreshTurtleList();
	RefreshQueue();
	RefreshPreview();
}

void UGatherAndHuntPlannerWidget::RefreshInfo()
{
	if (Session == nullptr || !Session->IsPuzzleLoaded())
	{
		PuzzleNameText->SetText(FText::FromString(TEXT("No puzzle loaded")));
		StatusText->SetText(FText::FromString(TEXT("Pick a board file below and press Load.")));
		IntroText->SetText(FText::GetEmpty());
		return;
	}

	PuzzleNameText->SetText(FText::FromString(Session->GetLoadedPuzzleName().IsEmpty() ? TEXT("(unnamed puzzle)") : Session->GetLoadedPuzzleName()));

	FString Status = FString::Printf(TEXT("Turn %d"), Session->GetCurrentTurnNumber() + 1);
	if (Session->GetTargetTurnCount() > 0)
	{
		Status += FString::Printf(TEXT(" / %d"), Session->GetTargetTurnCount());
	}
	Status += FString::Printf(TEXT("    Score %d"), Session->GetPlayerScore());
	if (Session->GetTargetScore() > 0)
	{
		Status += FString::Printf(TEXT(" / %d"), Session->GetTargetScore());
	}
	if (Session->GetTargetObjectiveCount() > 0)
	{
		Status += FString::Printf(TEXT("    Objectives needed %d"), Session->GetTargetObjectiveCount());
	}
	Status += FString::Printf(TEXT("    [%s]"), OutcomeLabel(Session->GetOutcome()));
	const int32 BoardTurn = Session->GetCurrentTurnNumber();
	Status += FString::Printf(TEXT("\nScripted opponent this turn: %s"), Session->HasScriptedOpponentTurn(BoardTurn) ? TEXT("yes") : TEXT("none"));
	if (Planner != nullptr && Planner->HasUnsavedOpponentDraft())
	{
		Status += TEXT("   (draft not saved to turn)");
	}
	if (Session->HasUnsavedChanges())
	{
		Status += TEXT("   * board file has unsaved changes");
	}
	StatusText->SetText(FText::FromString(Status));
	IntroText->SetText(FText::FromString(Session->GetIntroText()));
}

void UGatherAndHuntPlannerWidget::RefreshTurtleList()
{
	TurtleList->ClearChildren();
	if (Planner == nullptr || !Planner->HasBoard())
	{
		return;
	}

	const TArray<FGatherAndHuntPlannerTurtleSummary> Summaries = Planner->GetTurtleSummaries();
	if (Summaries.Num() == 0)
	{
		TurtleList->AddChildToVerticalBox(MakeText(TEXT("No playable turtles."), 11, DimTextColor));
		return;
	}

	for (int32 Index = 0; Index < Summaries.Num(); ++Index)
	{
		const FGatherAndHuntPlannerTurtleSummary& Summary = Summaries[Index];
		FString Label = FString::Printf(TEXT("%d  T%d  (%d,%d,%d) %s"),
			Index + 1, Summary.TurtleId,
			Summary.GridPosition.X, Summary.GridPosition.Y, Summary.GridPosition.Z,
			*FacingLabel(Summary.Heading, Summary.Pitch));
		if (Summary.CarriedPoints > 0)
		{
			Label += FString::Printf(TEXT("  carrying %d"), Summary.CarriedPoints);
		}
		Label += FString::Printf(TEXT("\n     %d moves, cost %d"), Summary.MoveCount, Summary.PlanCost);
		if (Summary.HighestCostTier >= 2)
		{
			Label += FString::Printf(TEXT("  [tier %d]"), Summary.HighestCostTier);
		}
		if (Summary.PreviewClaimedPoints > 0)
		{
			Label += FString::Printf(TEXT("  +%d"), Summary.PreviewClaimedPoints);
		}
		Label += FateLabel(Summary.PreviewFate);

		UGatherAndHuntPlannerButton* const Button = MakeButton(Label, Summary.TurtleId, &UGatherAndHuntPlannerWidget::OnTurtleButton, 10);
		if (Summary.bHasValidationError)
		{
			Button->SetBackgroundColor(ErrorColor);
		}
		else if (Summary.TurtleId == Planner->GetSelectedTurtleId())
		{
			Button->SetBackgroundColor(SelectedColor);
		}
		TurtleList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 2.0f));
	}
}

void UGatherAndHuntPlannerWidget::RefreshQueue()
{
	QueueStrip->ClearChildren();
	if (Planner == nullptr || !Planner->HasBoard() || Planner->GetSelectedTurtleId() == INDEX_NONE)
	{
		QueueHeaderText->SetText(FText::FromString(TEXT("QUEUE")));
		BudgetBar->SetPercent(0.0f);
		BudgetText->SetText(FText::GetEmpty());
		return;
	}

	const int32 TurtleId = Planner->GetSelectedTurtleId();
	const TArray<EGatherAndHuntPuzzleMoveKind> Moves = Planner->GetTurtleMoves(TurtleId);
	const FGatherAndHuntPuzzlePlanValidation Validation = Planner->GetValidation();
	const int32 SelectedMove = Planner->GetSelectedMoveIndex();

	QueueHeaderText->SetText(FText::FromString(FString::Printf(
		TEXT("%s  T%d   %s"),
		Planner->IsEditingOpponent() ? TEXT("OPPONENT QUEUE") : TEXT("QUEUE"),
		TurtleId,
		SelectedMove == INDEX_NONE ? TEXT("(new moves append)") : *FString::Printf(TEXT("(new moves insert after #%d)"), SelectedMove + 1))));

	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		const int32 Cost = UGatherAndHuntTurnPlanner::GetMoveCost(Index);
		UGatherAndHuntPlannerButton* const Chip = MakeButton(
			FString::Printf(TEXT("%d %s  %d"), Index + 1, MoveLabel(Moves[Index]), Cost),
			Index,
			&UGatherAndHuntPlannerWidget::OnQueueChip,
			10);

		const bool bOffending = !Validation.bIsValid && Validation.OffendingTurtleId == TurtleId && Validation.OffendingMoveIndex == Index;
		Chip->SetBackgroundColor(bOffending ? ErrorColor
			: (Index == SelectedMove ? SelectedColor : TierColors[UGatherAndHuntTurnPlanner::GetMoveCostTier(Index)]));
		QueueStrip->AddChildToWrapBox(Chip);
	}
	if (Moves.Num() == 0)
	{
		QueueStrip->AddChildToWrapBox(MakeText(TEXT("No moves queued. Use the buttons or W/A/D/R/F/S."), 10, DimTextColor));
	}

	const int32 Total = Planner->GetTotalCost();
	const int32 Budget = FMath::Max(1, Planner->GetBudget());
	BudgetBar->SetPercent(FMath::Clamp(static_cast<float>(Total) / Budget, 0.0f, 1.0f));
	BudgetBar->SetFillColorAndOpacity(Total > Budget ? ErrorColor : (Total * 4 >= Budget * 3 ? FLinearColor(1.0f, 0.7f, 0.1f) : GoodColor));
	BudgetText->SetText(FText::FromString(FString::Printf(TEXT("  Budget %d / %d"), Total, Budget)));
}

void UGatherAndHuntPlannerWidget::RefreshPreview()
{
	const bool bPlaying = BoardView != nullptr && BoardView->IsPlayingBack();
	const bool bLoaded = Session != nullptr && Session->IsPuzzleLoaded();
	const bool bFinished = bLoaded && (Session->GetOutcome() == EGatherAndHuntPuzzleOutcome::Victory || Session->GetOutcome() == EGatherAndHuntPuzzleOutcome::Defeat);

	if (OpponentPreviewCheck != nullptr && Planner != nullptr)
	{
		OpponentPreviewCheck->SetIsChecked(Planner->GetPreviewIncludesOpponent());
	}

	const bool bEditingOpponent = Planner != nullptr && Planner->IsEditingOpponent();
	SideLabel->SetText(FText::FromString(bEditingOpponent
		? TEXT("Editing: OPPONENT  (O: back to player)")
		: TEXT("Editing: PLAYER  (O: script opponent)")));
	SideButton->SetBackgroundColor(bEditingOpponent ? FLinearColor(0.8f, 0.1f, 0.6f) : FLinearColor::White);
	SubmitLabel->SetText(FText::FromString(bEditingOpponent
		? FString::Printf(TEXT("  SAVE OPPONENT TURN %d (Enter)  "), (Planner != nullptr ? Planner->GetBoardTurnNumber() : 0) + 1)
		: FString(TEXT("  SUBMIT TURN (Enter)  "))));
	RevertButton->SetVisibility(bEditingOpponent ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RevertButton->SetIsEnabled(Planner != nullptr && Planner->HasUnsavedOpponentDraft());

	if (Planner == nullptr || !Planner->HasBoard())
	{
		PreviewText->SetText(FText::GetEmpty());
		ValidationText->SetText(FText::GetEmpty());
		SubmitButton->SetIsEnabled(false);
		return;
	}

	const FGatherAndHuntPlannerPreviewSummary Summary = Planner->GetPreviewSummary();
	FString Text = FString::Printf(TEXT("Score %d -> %d"), Summary.ScoreBefore, Summary.ScoreAfter);
	if (Summary.ScoreAfter != Summary.ScoreBefore)
	{
		Text += FString::Printf(TEXT("  (%+d)"), Summary.ScoreAfter - Summary.ScoreBefore);
	}
	Text += FString::Printf(TEXT("\n%d claimed   %d delivered   %d lost\n%d ticks"),
		Summary.ObjectivesClaimed, Summary.TurtlesDelivered, Summary.TurtlesLost, Summary.TickCount);
	PreviewText->SetText(FText::FromString(Text));
	PreviewText->SetColorAndOpacity(FSlateColor(Summary.TurtlesLost > 0 ? ErrorColor : FLinearColor::White));

	const FGatherAndHuntPuzzlePlanValidation Validation = Planner->GetValidation();
	FString Message;
	if (bPlaying)
	{
		Message = TEXT("Playing turn... (Enter to skip)");
	}
	else if (bFinished && !bEditingOpponent)
	{
		Message = FString::Printf(TEXT("%s. Press Reset to replay the puzzle."), OutcomeLabel(Session->GetOutcome()));
	}
	else if (!Validation.bIsValid)
	{
		Message = Validation.ErrorMessage;
		if (Validation.OffendingTurtleId != INDEX_NONE)
		{
			Message += FString::Printf(TEXT(" (T%d"), Validation.OffendingTurtleId);
			Message += Validation.OffendingMoveIndex != INDEX_NONE ? FString::Printf(TEXT(", move %d)"), Validation.OffendingMoveIndex + 1) : TEXT(")");
		}
	}
	ValidationText->SetText(FText::FromString(Message));
	ValidationText->SetColorAndOpacity(FSlateColor(bFinished && Session->GetOutcome() == EGatherAndHuntPuzzleOutcome::Victory ? GoodColor : ErrorColor));
	SubmitButton->SetIsEnabled(bEditingOpponent
		? Validation.bIsValid && !bPlaying
		: Planner->CanSubmit() && !bPlaying && !bFinished);
}

void UGatherAndHuntPlannerWidget::RefreshPuzzleFiles()
{
	if (PuzzleFileCombo == nullptr)
	{
		return;
	}
	const FString Previous = PuzzleFileCombo->GetSelectedOption();
	PuzzleFileCombo->ClearOptions();

	const FString Dir = BoardSetupsDir();
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.dat"), true, false);
	Files.Sort();
	for (FString& File : Files)
	{
		FPaths::MakePathRelativeTo(File, *(Dir + TEXT("/")));
		PuzzleFileCombo->AddOption(File);
	}

	FString Current = Previous;
	if (Current.IsEmpty() && Session != nullptr && Session->IsPuzzleLoaded())
	{
		if (const AGatherAndHuntPuzzleGameMode* const GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>() : nullptr)
		{
			Current = GameMode->AutoLoadPuzzleRelativeFilename;
			if (!Current.IsEmpty() && !Current.EndsWith(TEXT(".dat")))
			{
				Current += TEXT(".dat");
			}
		}
	}
	if (!Current.IsEmpty() && PuzzleFileCombo->FindOptionIndex(Current) != INDEX_NONE)
	{
		PuzzleFileCombo->SetSelectedOption(Current);
	}
	else if (PuzzleFileCombo->GetOptionCount() > 0)
	{
		PuzzleFileCombo->SetSelectedIndex(0);
	}
}

// --- Commands ---------------------------------------------------------------

void UGatherAndHuntPlannerWidget::OnMoveButton(const int32 MoveKind)
{
	if (Planner != nullptr && (BoardView == nullptr || !BoardView->IsPlayingBack()))
	{
		Planner->AddMove(static_cast<EGatherAndHuntPuzzleMoveKind>(MoveKind));
	}
}

void UGatherAndHuntPlannerWidget::OnEditButton(const int32 Command)
{
	if (Planner == nullptr)
	{
		return;
	}
	switch (static_cast<EEditCommand>(Command))
	{
	case EEditCommand::ShiftLeft: Planner->ShiftSelectedMove(-1); break;
	case EEditCommand::ShiftRight: Planner->ShiftSelectedMove(1); break;
	case EEditCommand::Delete: Planner->RemoveSelectedMove(); break;
	case EEditCommand::ClearTurtle: Planner->ClearSelectedTurtle(); break;
	case EEditCommand::ClearAll: Planner->ClearAll(); break;
	case EEditCommand::Submit: SubmitPlan(); break;
	case EEditCommand::SkipPlayback: if (BoardView != nullptr) { BoardView->SkipPlayback(); } break;
	case EEditCommand::FrameBoard: if (BoardView != nullptr) { BoardView->FrameBoard(); } break;
	case EEditCommand::ToggleSide: Planner->SetEditingOpponent(!Planner->IsEditingOpponent()); break;
	case EEditCommand::RevertOpponent: Planner->RevertOpponentDraft(); ShowMessage(TEXT("Opponent draft reverted."), false); break;
	}
}

void UGatherAndHuntPlannerWidget::OnTurtleButton(const int32 TurtleId)
{
	if (Planner != nullptr)
	{
		Planner->SelectTurtle(TurtleId);
	}
}

void UGatherAndHuntPlannerWidget::OnQueueChip(const int32 MoveIndex)
{
	if (Planner != nullptr)
	{
		// Clicking the selected chip again moves the cursor back to the end.
		Planner->SelectMove(MoveIndex == Planner->GetSelectedMoveIndex() ? INDEX_NONE : MoveIndex);
	}
}

void UGatherAndHuntPlannerWidget::OnFileButton(const int32 Command)
{
	switch (static_cast<EFileCommand>(Command))
	{
	case EFileCommand::Load: LoadSelectedPuzzle(); break;
	case EFileCommand::Reset: ResetPuzzle(); break;
	case EFileCommand::RefreshList: RefreshPuzzleFiles(); break;
	case EFileCommand::Save: SavePuzzleFile(); break;
	}
}

void UGatherAndHuntPlannerWidget::SubmitPlan()
{
	if (Planner == nullptr || (BoardView != nullptr && BoardView->IsPlayingBack()))
	{
		return;
	}
	FGatherAndHuntPuzzlePlanValidation Result;
	if (Planner->IsEditingOpponent())
	{
		const int32 Turn = Planner->GetBoardTurnNumber() + 1;
		if (Planner->SaveOpponentTurn(Result))
		{
			ShowMessage(FString::Printf(TEXT("Saved opponent turn %d to the puzzle. Ctrl+S writes the file."), Turn), false);
		}
		else
		{
			ShowMessage(FString::Printf(TEXT("Opponent turn not saved: %s"), *Result.ErrorMessage), true);
		}
		Refresh();
		return;
	}

	// Success clears the plan and starts playback via the planner's events;
	// failure is reported through the validation text on the next refresh.
	Planner->Submit(Result);
	Refresh();
}

void UGatherAndHuntPlannerWidget::SavePuzzleFile()
{
	const FString File = GetLoadedPuzzleFile();
	if (Session == nullptr || !Session->IsPuzzleLoaded() || File.IsEmpty())
	{
		ShowMessage(TEXT("Nothing to save: no puzzle file is loaded."), true);
		return;
	}
	if (Planner != nullptr && Planner->HasUnsavedOpponentDraft())
	{
		ShowMessage(TEXT("Save or revert the opponent draft first (Enter / Revert)."), true);
		return;
	}
	const bool bSaved = Session->SavePuzzleToProjectFile(File);
	ShowMessage(bSaved ? FString::Printf(TEXT("Saved %s."), *File) : FString::Printf(TEXT("Could not save %s."), *File), !bSaved);
	Refresh();
}

void UGatherAndHuntPlannerWidget::LoadSelectedPuzzle()
{
	UWorld* const World = GetWorld();
	AGatherAndHuntPuzzleGameMode* const GameMode = World != nullptr ? World->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>() : nullptr;
	const FString File = PuzzleFileCombo != nullptr ? PuzzleFileCombo->GetSelectedOption() : FString();
	if (GameMode == nullptr || File.IsEmpty())
	{
		return;
	}

	if (!GameMode->LoadPuzzleFromRelativeFilename(File))
	{
		ShowMessage(FString::Printf(TEXT("Could not load %s."), *File), true);
		return;
	}
	ShowMessage(FString::Printf(TEXT("Loaded %s."), *File), false);
	GameMode->AutoLoadPuzzleRelativeFilename = File;

	// The GameMode keeps the same session object; rebind to reset plans and visuals.
	Session = GameMode->GetPuzzleSession();
	if (Planner != nullptr)
	{
		Planner->BindToSession(Session);
	}
	if (BoardView != nullptr)
	{
		BoardView->Bind(Session, Planner);
	}
	Refresh();
}

void UGatherAndHuntPlannerWidget::ResetPuzzle()
{
	UWorld* const World = GetWorld();
	AGatherAndHuntPuzzleGameMode* const GameMode = World != nullptr ? World->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>() : nullptr;
	if (GameMode == nullptr || !GameMode->ResetPuzzle())
	{
		return;
	}
	if (Planner != nullptr)
	{
		Planner->ClearAll();
		Planner->RefreshFromSession();
	}
	if (BoardView != nullptr)
	{
		BoardView->RebuildBoard();
	}
	Refresh();
}

// --- Events -----------------------------------------------------------------

void UGatherAndHuntPlannerWidget::HandlePlannerChanged()
{
	Refresh();
}

void UGatherAndHuntPlannerWidget::HandlePlaybackFinished()
{
	Refresh();
}

void UGatherAndHuntPlannerWidget::HandleOpponentPreviewToggled(const bool bIsChecked)
{
	if (Planner != nullptr)
	{
		Planner->SetPreviewIncludesOpponent(bIsChecked);
	}
}

// --- Input ------------------------------------------------------------------

FReply UGatherAndHuntPlannerWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (Planner == nullptr)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	const FKey Key = InKeyEvent.GetKey();
	const bool bPlaying = BoardView != nullptr && BoardView->IsPlayingBack();

	if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		if (bPlaying)
		{
			BoardView->SkipPlayback();
		}
		else if (Key == EKeys::Enter)
		{
			SubmitPlan();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Home)
	{
		OnEditButton(static_cast<int32>(EEditCommand::FrameBoard));
		return FReply::Handled();
	}
	if (Key == EKeys::S && InKeyEvent.IsControlDown())
	{
		SavePuzzleFile();
		return FReply::Handled();
	}
	if (Key == EKeys::O)
	{
		OnEditButton(static_cast<int32>(EEditCommand::ToggleSide));
		return FReply::Handled();
	}
	if (Key == EKeys::P)
	{
		Planner->SetPreviewIncludesOpponent(!Planner->GetPreviewIncludesOpponent());
		return FReply::Handled();
	}
	if (Key == EKeys::Tab)
	{
		Planner->SelectAdjacentTurtle(InKeyEvent.IsShiftDown() ? -1 : 1);
		return FReply::Handled();
	}

	static const FKey DigitKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(DigitKeys); ++Index)
	{
		if (Key == DigitKeys[Index])
		{
			const TArray<FGatherAndHuntPlannerTurtleSummary> Summaries = Planner->GetTurtleSummaries();
			if (Summaries.IsValidIndex(Index))
			{
				Planner->SelectTurtle(Summaries[Index].TurtleId);
			}
			return FReply::Handled();
		}
	}

	if (bPlaying)
	{
		return FReply::Handled();
	}

	struct FMoveKey { FKey Key; EGatherAndHuntPuzzleMoveKind Kind; };
	static const FMoveKey MoveKeys[] = {
		{EKeys::W, EGatherAndHuntPuzzleMoveKind::Forward},
		{EKeys::Up, EGatherAndHuntPuzzleMoveKind::Forward},
		{EKeys::A, EGatherAndHuntPuzzleMoveKind::YawLeft},
		{EKeys::Left, EGatherAndHuntPuzzleMoveKind::YawLeft},
		{EKeys::D, EGatherAndHuntPuzzleMoveKind::YawRight},
		{EKeys::Right, EGatherAndHuntPuzzleMoveKind::YawRight},
		{EKeys::R, EGatherAndHuntPuzzleMoveKind::PitchUp},
		{EKeys::F, EGatherAndHuntPuzzleMoveKind::PitchDown},
		{EKeys::S, EGatherAndHuntPuzzleMoveKind::Wait},
		{EKeys::Down, EGatherAndHuntPuzzleMoveKind::Wait}};
	for (const FMoveKey& MoveKey : MoveKeys)
	{
		if (Key == MoveKey.Key)
		{
			Planner->AddMove(MoveKey.Kind);
			return FReply::Handled();
		}
	}

	if (Key == EKeys::BackSpace || Key == EKeys::Delete)
	{
		Planner->RemoveSelectedMove();
	}
	else if (Key == EKeys::LeftBracket)
	{
		Planner->ShiftSelectedMove(-1);
	}
	else if (Key == EKeys::RightBracket)
	{
		Planner->ShiftSelectedMove(1);
	}
	else if (Key == EKeys::Escape)
	{
		Planner->SelectMove(INDEX_NONE);
	}
	else if (Key == EKeys::C)
	{
		InKeyEvent.IsShiftDown() ? Planner->ClearAll() : Planner->ClearSelectedTurtle();
	}
	else
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return FReply::Handled();
}

FReply UGatherAndHuntPlannerWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Reaches here only for clicks on empty space; buttons handle their own.
	bOrbiting = true;
	return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget());
}

FReply UGatherAndHuntPlannerWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bOrbiting)
	{
		bOrbiting = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UGatherAndHuntPlannerWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bOrbiting && BoardView != nullptr)
	{
		const FVector2D Delta = InMouseEvent.GetCursorDelta();
		BoardView->OrbitCamera(Delta.X * 0.3f, -Delta.Y * 0.3f);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UGatherAndHuntPlannerWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (BoardView != nullptr)
	{
		BoardView->ZoomCamera(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}
