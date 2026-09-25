#include "GatherAndHuntBoardEditorWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GatherAndHuntBoardEditor.h"
#include "GatherAndHuntFrontEnd.h"
#include "GatherAndHuntPuzzleBoardView.h"
#include "GatherAndHuntPuzzleGameMode.h"
#include "GatherAndHuntPuzzleHUD.h"
#include "GatherAndHuntPuzzleSession.h"
#include "GatherAndHuntWidgetBuilder.h"

using namespace GatherAndHuntUI;

namespace
{
	enum class EField : int32
	{
		TargetScore,
		TargetTurns,
		TargetObjectives,
		Act,
		Order,
		SizeX,
		SizeY,
		SizeZ,
		HazardLength,
		HazardWidth,
		HazardStride,
		HazardPhase,
		ShipsPerTurn
	};

	// Stepper payloads carry the field and the direction.
	constexpr int32 StepUpFlag = 0x100;

	enum class EAction : int32
	{
		Save,
		New,
		Undo,
		Redo,
		Delete,
		RotateLeft,
		RotateRight,
		PitchUp,
		PitchDown,
		RolePlayer,
		RoleAlly,
		RoleOpponent,
		Planner,
		Menu,
		Frame,
		ToggleGoal,
		ToggleGround
	};

	const EGatherAndHuntPuzzleMoveKind AllMoveKinds[] = {
		EGatherAndHuntPuzzleMoveKind::Forward, EGatherAndHuntPuzzleMoveKind::YawLeft, EGatherAndHuntPuzzleMoveKind::YawRight,
		EGatherAndHuntPuzzleMoveKind::PitchUp, EGatherAndHuntPuzzleMoveKind::PitchDown, EGatherAndHuntPuzzleMoveKind::Wait};

	const TCHAR* MoveToggleLabel(const EGatherAndHuntPuzzleMoveKind Kind)
	{
		switch (Kind)
		{
		case EGatherAndHuntPuzzleMoveKind::Forward: return TEXT("Fwd");
		case EGatherAndHuntPuzzleMoveKind::YawLeft: return TEXT("Left");
		case EGatherAndHuntPuzzleMoveKind::YawRight: return TEXT("Right");
		case EGatherAndHuntPuzzleMoveKind::PitchUp: return TEXT("Up");
		case EGatherAndHuntPuzzleMoveKind::PitchDown: return TEXT("Down");
		default: return TEXT("Wait");
		}
	}

	// Pixels the pointer may travel and still count as a click rather than an orbit.
	constexpr float ClickThreshold = 5.0f;

	const FIntVector DefaultNewBoardSize(6, 6, 3);

	struct FPaletteEntry
	{
		EGatherAndHuntBoardPieceKind Kind;
		const TCHAR* Label;
		FLinearColor Color;
	};

	// Colours match the board view's pieces.
	const FPaletteEntry Palette[] = {
		{EGatherAndHuntBoardPieceKind::Turtle, TEXT("Turtle"), FLinearColor(0.15f, 0.8f, 0.3f)},
		{EGatherAndHuntBoardPieceKind::Objective, TEXT("Objective (+1)"), FLinearColor(1.0f, 0.8f, 0.0f)},
		{EGatherAndHuntBoardPieceKind::GoldObjective, TEXT("Gold objective (+5)"), FLinearColor(1.0f, 0.8f, 0.0f)},
		{EGatherAndHuntBoardPieceKind::Block, TEXT("Block"), FLinearColor(0.35f, 0.35f, 0.38f)},
		{EGatherAndHuntBoardPieceKind::Hunter, TEXT("Hunter"), FLinearColor(0.9f, 0.05f, 0.05f)},
		{EGatherAndHuntBoardPieceKind::PatrolHazard, TEXT("Patrol hazard"), FLinearColor(1.0f, 0.45f, 0.0f)},
		{EGatherAndHuntBoardPieceKind::LoopHazard, TEXT("Loop hazard"), FLinearColor(1.0f, 0.45f, 0.0f)}};

	const TCHAR* KindLabel(const EGatherAndHuntBoardPieceKind Kind)
	{
		for (const FPaletteEntry& Entry : Palette)
		{
			if (Entry.Kind == Kind) return Entry.Label;
		}
		return TEXT("?");
	}

	const TCHAR* RoleLabel(const EGatherAndHuntTurtleRole Role)
	{
		switch (Role)
		{
		case EGatherAndHuntTurtleRole::Ally: return TEXT("Ally");
		case EGatherAndHuntTurtleRole::Opponent: return TEXT("Opponent");
		default: return TEXT("Player");
		}
	}

	FString FacingLabel(const int32 Heading, const int32 Pitch)
	{
		static const TCHAR* Headings[] = {TEXT("North"), TEXT("East"), TEXT("South"), TEXT("West")};
		const FString HeadingName = Headings[FMath::Clamp(Heading, 0, 3)];
		if (Pitch == 0) return HeadingName + TEXT(", down");
		if (Pitch == 2) return HeadingName + TEXT(", up");
		return HeadingName;
	}

	FString TargetLabel(const int32 Value)
	{
		return Value > 0 ? FString::FromInt(Value) : FString(TEXT("-"));
	}
}

UGatherAndHuntBoardEditorWidget::UGatherAndHuntBoardEditorWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UGatherAndHuntBoardEditorWidget::RebuildWidget()
{
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UGatherAndHuntBoardEditorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Bind may run before the tree exists (and the widget is re-added on every
	// switch from the planner), so fill it in once it's on screen.
	Refresh();
}

void UGatherAndHuntBoardEditorWidget::Bind(
	UGatherAndHuntPuzzleSession* InSession,
	UGatherAndHuntBoardEditor* InEditor,
	AGatherAndHuntPuzzleBoardView* InBoardView)
{
	if (Editor != nullptr)
	{
		Editor->OnChanged.RemoveDynamic(this, &UGatherAndHuntBoardEditorWidget::HandleEditorChanged);
	}
	Session = InSession;
	Editor = InEditor;
	BoardView = InBoardView;
	if (Editor != nullptr)
	{
		Editor->OnChanged.AddDynamic(this, &UGatherAndHuntBoardEditorWidget::HandleEditorChanged);
	}
	Refresh();
}

void UGatherAndHuntBoardEditorWidget::SetTargetFile(const FString& RelativeFilename)
{
	TargetFile = UGatherAndHuntFrontEnd::NormalizePuzzleFilename(RelativeFilename);
	PendingOverwriteFile.Reset();
	if (FileBox != nullptr)
	{
		FileBox->SetText(FText::FromString(TargetFile));
	}
}

void UGatherAndHuntBoardEditorWidget::ShowMessage(const FString& Message, const bool bIsError)
{
	if (MessageText != nullptr)
	{
		MessageText->SetText(FText::FromString(Message));
		MessageText->SetColorAndOpacity(FSlateColor(bIsError ? ErrorColor : GoodColor));
	}
}

// --- Tree construction ----------------------------------------------------------

UWidget* UGatherAndHuntBoardEditorWidget::MakeStepper(const FString& Label, const int32 Field, TObjectPtr<UTextBlock>& OutValue)
{
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (!Label.IsEmpty())
	{
		AddCell(Row, MakeText(WidgetTree, Label, 10, DimTextColor));
	}
	AddCell(Row, MakeButton(WidgetTree, this, TEXT("-"), Field, &UGatherAndHuntBoardEditorWidget::OnStepper, 10), 2.0f);
	OutValue = MakeText(WidgetTree, TEXT("0"), 11);
	OutValue->SetJustification(ETextJustify::Center);
	AddCell(Row, SizeTo(WidgetTree, OutValue, 28.0f), 2.0f);
	AddCell(Row, MakeButton(WidgetTree, this, TEXT("+"), Field | StepUpFlag, &UGatherAndHuntBoardEditorWidget::OnStepper, 10), 10.0f);
	return Row;
}

void UGatherAndHuntBoardEditorWidget::BuildTree()
{
	// Transparent full-screen backdrop catches clicks, drags and the wheel on
	// empty space; panels sit on a canvas on top of it.
	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(FLinearColor::Transparent);
	Backdrop->SetPadding(FMargin(0.0f));
	Backdrop->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Backdrop;

	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Backdrop->SetContent(Canvas);

	auto Action = [this](const FString& Label, const EAction InAction, const int32 FontSize = 11)
	{
		return MakeButton(WidgetTree, this, Label, static_cast<int32>(InAction), &UGatherAndHuntBoardEditorWidget::OnAction, FontSize);
	};

	// --- Top left: puzzle metadata and file ---------------------------------
	{
		UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Box, MakeText(WidgetTree, TEXT("BOARD EDITOR"), 16), 0.0f);
		StatusText = MakeText(WidgetTree, TEXT(""), 10, DimTextColor);
		AddRow(Box, StatusText);

		UHorizontalBox* const NameRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(NameRow, SizeTo(WidgetTree, MakeText(WidgetTree, TEXT("Name"), 10, DimTextColor), 40.0f));
		NameBox = WidgetTree->ConstructWidget<UEditableTextBox>();
		SetFontSize(NameBox, 11);
		NameBox->OnTextCommitted.AddDynamic(this, &UGatherAndHuntBoardEditorWidget::HandleNameCommitted);
		AddCell(NameRow, SizeTo(WidgetTree, NameBox, 300.0f));
		AddRow(Box, NameRow, 8.0f);

		UHorizontalBox* const FileRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(FileRow, SizeTo(WidgetTree, MakeText(WidgetTree, TEXT("File"), 10, DimTextColor), 40.0f));
		FileBox = WidgetTree->ConstructWidget<UEditableTextBox>();
		FileBox->SetHintText(FText::FromString(TEXT("e.g. Act1/Crossing")));
		SetFontSize(FileBox, 11);
		FileBox->OnTextCommitted.AddDynamic(this, &UGatherAndHuntBoardEditorWidget::HandleFileCommitted);
		AddCell(FileRow, SizeTo(WidgetTree, FileBox, 200.0f));
		AddCell(FileRow, Action(TEXT("Save (Ctrl+S)"), EAction::Save));
		AddCell(FileRow, Action(TEXT("New"), EAction::New));
		AddRow(Box, FileRow, 4.0f);

		UHorizontalBox* const Targets = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Targets, MakeStepper(TEXT("Target score"), static_cast<int32>(EField::TargetScore), TargetScoreValue));
		AddCell(Targets, MakeStepper(TEXT("Turns"), static_cast<int32>(EField::TargetTurns), TargetTurnsValue));
		AddCell(Targets, MakeStepper(TEXT("Objectives"), static_cast<int32>(EField::TargetObjectives), TargetObjectivesValue));
		AddRow(Box, Targets, 8.0f);

		UHorizontalBox* const Ordering = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Ordering, MakeStepper(TEXT("Act"), static_cast<int32>(EField::Act), ActValue));
		AddCell(Ordering, MakeStepper(TEXT("Order"), static_cast<int32>(EField::Order), OrderValue));
		AddRow(Box, Ordering, 4.0f);

		UHorizontalBox* const Size = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Size, MakeText(WidgetTree, TEXT("Board"), 10, DimTextColor));
		AddCell(Size, MakeStepper(TEXT("X"), static_cast<int32>(EField::SizeX), SizeXValue));
		AddCell(Size, MakeStepper(TEXT("Y"), static_cast<int32>(EField::SizeY), SizeYValue));
		AddCell(Size, MakeStepper(TEXT("Z"), static_cast<int32>(EField::SizeZ), SizeZValue));
		AddRow(Box, Size, 4.0f);

		UHorizontalBox* const Rules = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Rules, MakeText(WidgetTree, TEXT("Rules"), 10, DimTextColor));
		GoalButton = Action(TEXT(""), EAction::ToggleGoal, 10);
		GroundButton = Action(TEXT(""), EAction::ToggleGround, 10);
		AddCell(Rules, GoalButton);
		AddCell(Rules, GroundButton, 10.0f);
		AddCell(Rules, MakeStepper(TEXT("Ships moved / turn"), static_cast<int32>(EField::ShipsPerTurn), ShipsPerTurnValue));
		AddRow(Box, Rules, 8.0f);

		UHorizontalBox* const MoveRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(MoveRow, MakeText(WidgetTree, TEXT("Player moves"), 10, DimTextColor));
		MoveToggles.Reset();
		for (const EGatherAndHuntPuzzleMoveKind Kind : AllMoveKinds)
		{
			UGatherAndHuntPlannerButton* const Toggle = MakeButton(WidgetTree, this, MoveToggleLabel(Kind), static_cast<int32>(Kind),
				&UGatherAndHuntBoardEditorWidget::OnMoveToggle, 10);
			MoveToggles.Add(Toggle);
			AddCell(MoveRow, Toggle, 2.0f);
		}
		AddRow(Box, MoveRow, 4.0f);

		AddRow(Box, MakeText(WidgetTree, TEXT("Intro text"), 10, DimTextColor), 8.0f);
		IntroBox = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>();
		SetFontSize(IntroBox, 10);
		IntroBox->OnTextCommitted.AddDynamic(this, &UGatherAndHuntBoardEditorWidget::HandleIntroCommitted);
		AddRow(Box, SizeTo(WidgetTree, IntroBox, 440.0f, 64.0f));

		MessageText = MakeText(WidgetTree, TEXT(""), 10, GoodColor);
		MessageText->SetAutoWrapText(true);
		AddRow(Box, SizeTo(WidgetTree, MessageText, 440.0f), 6.0f);

		Place(Canvas, MakePanel(WidgetTree, Box), FVector2D(0.0f, 0.0f), FVector2D(16.0f, 16.0f));
	}

	// --- Left: palette ----------------------------------------------------------
	{
		UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Box, MakeText(WidgetTree, TEXT("PLACE AT CURSOR  (1-7)"), 10, DimTextColor), 0.0f);
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Palette); ++Index)
		{
			UBorder* const Swatch = WidgetTree->ConstructWidget<UBorder>();
			Swatch->SetBrushColor(Palette[Index].Color);
			UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			Row->AddChildToHorizontalBox(SizeTo(WidgetTree, Swatch, 8.0f))->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
			Row->AddChildToHorizontalBox(MakeButton(WidgetTree, this, FString::Printf(TEXT("%d  %s"), Index + 1, Palette[Index].Label),
				static_cast<int32>(Palette[Index].Kind), &UGatherAndHuntBoardEditorWidget::OnPaletteButton, 10))
				->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			AddRow(Box, Row, 3.0f);
		}
		Place(Canvas, SizeTo(WidgetTree, MakePanel(WidgetTree, Box), 220.0f), FVector2D(0.0f, 0.62f), FVector2D(16.0f, 0.0f));
	}

	// --- Right: selection, issues, help -------------------------------------
	{
		UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Box, MakeText(WidgetTree, TEXT("CURSOR"), 10, DimTextColor), 0.0f);
		SelectionText = MakeText(WidgetTree, TEXT(""), 11);
		AddRow(Box, SelectionText);

		UHorizontalBox* const Common = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Common, Action(TEXT("Rotate L"), EAction::RotateLeft, 10));
		AddCell(Common, Action(TEXT("Rotate R"), EAction::RotateRight, 10));
		AddCell(Common, Action(TEXT("Delete"), EAction::Delete, 10));
		AddRow(Box, Common, 6.0f);

		TurtleControls = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBox* const Pitch = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Pitch, Action(TEXT("Pitch up (T)"), EAction::PitchUp, 10));
		AddCell(Pitch, Action(TEXT("Pitch down (G)"), EAction::PitchDown, 10));
		AddRow(TurtleControls, Pitch, 0.0f);
		UHorizontalBox* const Roles = WidgetTree->ConstructWidget<UHorizontalBox>();
		const EAction RoleActions[] = {EAction::RolePlayer, EAction::RoleAlly, EAction::RoleOpponent};
		const EGatherAndHuntTurtleRole Roles3[] = {EGatherAndHuntTurtleRole::Player, EGatherAndHuntTurtleRole::Ally, EGatherAndHuntTurtleRole::Opponent};
		RoleButtons.Reset();
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UGatherAndHuntPlannerButton* const Button = Action(RoleLabel(Roles3[Index]), RoleActions[Index], 10);
			RoleButtons.Add(Button);
			AddCell(Roles, Button);
		}
		AddRow(TurtleControls, Roles, 4.0f);
		AddRow(Box, TurtleControls, 4.0f);

		HazardControls = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(HazardControls, MakeStepper(TEXT("Length"), static_cast<int32>(EField::HazardLength), LengthValue), 0.0f);
		WidthStepper = MakeStepper(TEXT("Width "), static_cast<int32>(EField::HazardWidth), WidthValue);
		AddRow(HazardControls, WidthStepper);
		AddRow(HazardControls, MakeStepper(TEXT("Stride"), static_cast<int32>(EField::HazardStride), StrideValue));
		AddRow(HazardControls, MakeStepper(TEXT("Phase "), static_cast<int32>(EField::HazardPhase), PhaseValue));
		AddRow(Box, HazardControls, 4.0f);

		AddRow(Box, MakeText(WidgetTree, TEXT("ISSUES"), 10, DimTextColor), 12.0f);
		IssueList = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Box, IssueList);

		AddRow(Box, MakeText(WidgetTree,
			TEXT("WASD / arrows move   Q/E layer   Shift: carry piece\n")
			TEXT("1-7 place   Del remove   R rotate   T/G pitch   C role\n")
			TEXT("Ctrl+Z undo   Ctrl+Y redo   Ctrl+S save   B planner\n")
			TEXT("Click: cursor   Drag: orbit   Wheel: zoom   Home: reframe"),
			9, DimTextColor), 12.0f);

		Place(Canvas, SizeTo(WidgetTree, MakePanel(WidgetTree, Box), 330.0f), FVector2D(1.0f, 0.0f), FVector2D(-16.0f, 16.0f));
	}

	// --- Bottom: mode and history -------------------------------------------
	{
		UHorizontalBox* const Bar = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Bar, Action(TEXT("  PLAY / PLAN (B)  "), EAction::Planner, 13), 12.0f);
		UndoButton = Action(TEXT("Undo"), EAction::Undo);
		RedoButton = Action(TEXT("Redo"), EAction::Redo);
		AddCell(Bar, UndoButton);
		AddCell(Bar, RedoButton, 12.0f);
		AddCell(Bar, Action(TEXT("Reframe"), EAction::Frame));
		AddCell(Bar, Action(TEXT("Main menu"), EAction::Menu));
		Place(Canvas, MakePanel(WidgetTree, Bar), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -16.0f));
	}
}

// --- Refresh --------------------------------------------------------------------

void UGatherAndHuntBoardEditorWidget::Refresh()
{
	if (StatusText == nullptr)
	{
		return; // Tree not built.
	}
	RefreshPuzzle();
	RefreshSelection();
	RefreshIssues();
}

void UGatherAndHuntBoardEditorWidget::RefreshPuzzle()
{
	const bool bHasPuzzle = Editor != nullptr && Editor->HasPuzzle();
	FString Status = bHasPuzzle ? FString::Printf(TEXT("Editing the starting layout. %s"),
		Session != nullptr && Session->HasUnsavedChanges() ? TEXT("* unsaved changes") : TEXT("Saved."))
		: FString(TEXT("No puzzle loaded. Press New to start one."));
	StatusText->SetText(FText::FromString(Status));

	// Don't overwrite a box the user is typing in.
	if (!NameBox->HasKeyboardFocus())
	{
		NameBox->SetText(FText::FromString(bHasPuzzle ? Editor->GetPuzzleName() : FString()));
	}
	if (!IntroBox->HasKeyboardFocus())
	{
		IntroBox->SetText(FText::FromString(bHasPuzzle ? Editor->GetIntroText() : FString()));
	}
	if (!FileBox->HasKeyboardFocus())
	{
		FileBox->SetText(FText::FromString(TargetFile));
	}

	auto SetValue = [](UTextBlock* Text, const FString& Value) { Text->SetText(FText::FromString(Value)); };
	SetValue(TargetScoreValue, bHasPuzzle ? TargetLabel(Editor->GetTargetScore()) : TEXT("-"));
	SetValue(TargetTurnsValue, bHasPuzzle ? TargetLabel(Editor->GetTargetTurnCount()) : TEXT("-"));
	SetValue(TargetObjectivesValue, bHasPuzzle ? TargetLabel(Editor->GetTargetObjectiveCount()) : TEXT("-"));
	SetValue(ActValue, bHasPuzzle ? FString::FromInt(Editor->GetPuzzleAct()) : TEXT("-"));
	SetValue(OrderValue, bHasPuzzle ? FString::FromInt(Editor->GetPuzzleOrder()) : TEXT("-"));
	const FIntVector Count = bHasPuzzle ? Editor->GetCellCount() : FIntVector::ZeroValue;
	SetValue(SizeXValue, FString::FromInt(Count.X));
	SetValue(SizeYValue, FString::FromInt(Count.Y));
	SetValue(SizeZValue, FString::FromInt(Count.Z));

	Cast<UTextBlock>(GoalButton->GetContent())->SetText(FText::FromString(
		bHasPuzzle && Editor->GetGoal() == EGatherAndHuntPuzzleGoal::Reach ? TEXT("Goal: reach targets") : TEXT("Goal: deliver targets")));
	Cast<UTextBlock>(GroundButton->GetContent())->SetText(FText::FromString(
		bHasPuzzle && Editor->GetShowGround() ? TEXT("Grass: on") : TEXT("Grass: off")));
	SetValue(ShipsPerTurnValue, bHasPuzzle && Editor->GetMaxTurtlesMovedPerTurn() > 0 ? FString::FromInt(Editor->GetMaxTurtlesMovedPerTurn()) : TEXT("all"));
	for (UGatherAndHuntPlannerButton* const Toggle : MoveToggles)
	{
		const bool bAllowed = bHasPuzzle && Editor->IsMoveAllowed(static_cast<EGatherAndHuntPuzzleMoveKind>(Toggle->Payload));
		Toggle->SetBackgroundColor(bAllowed ? GoodColor : FLinearColor(0.35f, 0.35f, 0.38f));
	}

	UndoButton->SetIsEnabled(Editor != nullptr && Editor->CanUndo());
	RedoButton->SetIsEnabled(Editor != nullptr && Editor->CanRedo());
}

void UGatherAndHuntBoardEditorWidget::RefreshSelection()
{
	if (Editor == nullptr || !Editor->HasPuzzle())
	{
		SelectionText->SetText(FText::GetEmpty());
		TurtleControls->SetVisibility(ESlateVisibility::Collapsed);
		HazardControls->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FIntVector CursorCell = Editor->GetCursor();
	FString Text = FString::Printf(TEXT("Cell (%d, %d, %d)"), CursorCell.X, CursorCell.Y, CursorCell.Z);
	FGatherAndHuntBoardEditorPiece Piece;
	const bool bHasPiece = Editor->GetSelectedPiece(Piece);
	const bool bTurtle = bHasPiece && Piece.Kind == EGatherAndHuntBoardPieceKind::Turtle;
	const bool bHazard = bHasPiece && UGatherAndHuntBoardEditor::IsMovingHazard(Piece.Kind);
	if (!bHasPiece)
	{
		Text += TEXT("\nEmpty. Press 1-7 to place a piece.");
	}
	else
	{
		Text += FString::Printf(TEXT("\n#%d  %s"), Piece.PieceId, KindLabel(Piece.Kind));
		if (bTurtle)
		{
			Text += FString::Printf(TEXT("  (%s)\nFacing %s"), RoleLabel(Piece.Role), *FacingLabel(Piece.Heading, Piece.Pitch));
		}
		else if (bHazard)
		{
			Text += FString::Printf(TEXT("\nFacing %s   cycle %d turns"), *FacingLabel(Piece.Heading, 1), FMath::Max(1, Editor->GetHazardPath(Piece.PieceId).Num() - 1));
		}
	}
	SelectionText->SetText(FText::FromString(Text));

	TurtleControls->SetVisibility(bTurtle ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	HazardControls->SetVisibility(bHazard ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bTurtle)
	{
		const EGatherAndHuntTurtleRole Roles[] = {EGatherAndHuntTurtleRole::Player, EGatherAndHuntTurtleRole::Ally, EGatherAndHuntTurtleRole::Opponent};
		for (int32 Index = 0; Index < RoleButtons.Num(); ++Index)
		{
			RoleButtons[Index]->SetBackgroundColor(Piece.Role == Roles[Index]
				? AGatherAndHuntPuzzleBoardView::GetRoleColor(Roles[Index])
				: FLinearColor::White);
		}
	}
	if (bHazard)
	{
		LengthValue->SetText(FText::AsNumber(Piece.Length));
		WidthValue->SetText(FText::AsNumber(Piece.Width));
		StrideValue->SetText(FText::AsNumber(Piece.Stride));
		PhaseValue->SetText(FText::AsNumber(Piece.Phase));
		WidthStepper->SetVisibility(Piece.Kind == EGatherAndHuntBoardPieceKind::LoopHazard
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UGatherAndHuntBoardEditorWidget::RefreshIssues()
{
	IssueList->ClearChildren();
	if (Editor == nullptr || !Editor->HasPuzzle())
	{
		return;
	}
	const TArray<FLevelEditorValidationIssue> Issues = Editor->GetIssues();
	if (Issues.Num() == 0)
	{
		AddRow(IssueList, MakeText(WidgetTree, TEXT("None. The puzzle is playable."), 10, GoodColor), 0.0f);
		return;
	}
	for (const FLevelEditorValidationIssue& Issue : Issues)
	{
		const FString Line = Issue.PieceSaveID != INDEX_NONE
			? FString::Printf(TEXT("#%d  %s"), Issue.PieceSaveID, *Issue.Message)
			: Issue.Message;
		UTextBlock* const Text = MakeText(WidgetTree, Line, 10, Issue.bIsError ? ErrorColor : WarningColor);
		Text->SetAutoWrapText(true);
		AddRow(IssueList, Text, 1.0f);
	}
}

// --- Commands -------------------------------------------------------------------

void UGatherAndHuntBoardEditorWidget::OnPaletteButton(const int32 Kind)
{
	if (Editor == nullptr || !Editor->HasPuzzle())
	{
		return;
	}
	if (!Editor->PlacePiece(static_cast<EGatherAndHuntBoardPieceKind>(Kind)))
	{
		ShowMessage(TEXT("That cell is taken. Move the cursor or delete the piece first."), true);
	}
}

void UGatherAndHuntBoardEditorWidget::OnStepper(const int32 Payload)
{
	if (Editor == nullptr || !Editor->HasPuzzle())
	{
		return;
	}
	const int32 Delta = (Payload & StepUpFlag) != 0 ? 1 : -1;
	const FIntVector Count = Editor->GetCellCount();
	switch (static_cast<EField>(Payload & ~StepUpFlag))
	{
	case EField::TargetScore: Editor->SetTargetScore(Editor->GetTargetScore() + Delta); break;
	case EField::TargetTurns: Editor->SetTargetTurnCount(Editor->GetTargetTurnCount() + Delta); break;
	case EField::TargetObjectives: Editor->SetTargetObjectiveCount(Editor->GetTargetObjectiveCount() + Delta); break;
	case EField::Act: Editor->SetPuzzleActAndOrder(Editor->GetPuzzleAct() + Delta, Editor->GetPuzzleOrder()); break;
	case EField::Order: Editor->SetPuzzleActAndOrder(Editor->GetPuzzleAct(), Editor->GetPuzzleOrder() + Delta); break;
	case EField::SizeX:
	case EField::SizeY:
	case EField::SizeZ:
	{
		const int32 Axis = (Payload & ~StepUpFlag) - static_cast<int32>(EField::SizeX);
		FIntVector NewCount = Count;
		NewCount[Axis] += Delta;
		if (NewCount[Axis] >= 1 && !Editor->ResizeBoard(NewCount) && Delta < 0)
		{
			ShowMessage(TEXT("Pieces are in the row being removed. Move or delete them first."), true);
		}
		break;
	}
	case EField::HazardLength: Editor->AdjustSelectedHazard(EGatherAndHuntHazardParam::Length, Delta); break;
	case EField::HazardWidth: Editor->AdjustSelectedHazard(EGatherAndHuntHazardParam::Width, Delta); break;
	case EField::HazardStride: Editor->AdjustSelectedHazard(EGatherAndHuntHazardParam::Stride, Delta); break;
	case EField::HazardPhase: Editor->AdjustSelectedHazard(EGatherAndHuntHazardParam::Phase, Delta); break;
	case EField::ShipsPerTurn: Editor->SetMaxTurtlesMovedPerTurn(Editor->GetMaxTurtlesMovedPerTurn() + Delta); break;
	}
}

void UGatherAndHuntBoardEditorWidget::OnAction(const int32 InAction)
{
	const EAction Action = static_cast<EAction>(InAction);
	if (Action != EAction::New && Action != EAction::Menu)
	{
		PendingConfirmAction = INDEX_NONE;
	}
	if (Action == EAction::Save || Action == EAction::New || Action == EAction::Menu || Action == EAction::Frame || Action == EAction::Planner)
	{
		switch (Action)
		{
		case EAction::Save: Save(); break;
		case EAction::New: NewPuzzle(); break;
		case EAction::Menu: BackToMenu(); break;
		case EAction::Planner: BackToPlanner(); break;
		default: if (BoardView != nullptr) { BoardView->FrameBoard(); } break;
		}
		return;
	}
	if (Editor == nullptr || !Editor->HasPuzzle())
	{
		return;
	}
	switch (Action)
	{
	case EAction::Undo: Editor->Undo(); break;
	case EAction::Redo: Editor->Redo(); break;
	case EAction::Delete: Editor->RemoveSelectedPiece(); break;
	case EAction::RotateLeft: Editor->RotateSelectedPiece(-1); break;
	case EAction::RotateRight: Editor->RotateSelectedPiece(1); break;
	case EAction::PitchUp: Editor->PitchSelectedPiece(1); break;
	case EAction::PitchDown: Editor->PitchSelectedPiece(-1); break;
	case EAction::RolePlayer: Editor->SetSelectedTurtleRole(EGatherAndHuntTurtleRole::Player); break;
	case EAction::RoleAlly: Editor->SetSelectedTurtleRole(EGatherAndHuntTurtleRole::Ally); break;
	case EAction::RoleOpponent: Editor->SetSelectedTurtleRole(EGatherAndHuntTurtleRole::Opponent); break;
	case EAction::ToggleGoal:
		Editor->SetGoal(Editor->GetGoal() == EGatherAndHuntPuzzleGoal::Reach ? EGatherAndHuntPuzzleGoal::Deliver : EGatherAndHuntPuzzleGoal::Reach);
		break;
	case EAction::ToggleGround: Editor->SetShowGround(!Editor->GetShowGround()); break;
	default: break;
	}
}

void UGatherAndHuntBoardEditorWidget::OnMoveToggle(const int32 MoveKind)
{
	if (Editor != nullptr && Editor->HasPuzzle())
	{
		const EGatherAndHuntPuzzleMoveKind Kind = static_cast<EGatherAndHuntPuzzleMoveKind>(MoveKind);
		Editor->SetMoveAllowed(Kind, !Editor->IsMoveAllowed(Kind));
	}
}

bool UGatherAndHuntBoardEditorWidget::ConfirmDiscard(const int32 Action, const FString& What)
{
	if (Session == nullptr || !Session->HasUnsavedChanges() || PendingConfirmAction == Action)
	{
		PendingConfirmAction = INDEX_NONE;
		return true;
	}
	PendingConfirmAction = Action;
	ShowMessage(FString::Printf(TEXT("Unsaved changes. Press %s again to discard them."), *What), true);
	return false;
}

void UGatherAndHuntBoardEditorWidget::Save()
{
	if (Session == nullptr || Editor == nullptr || !Editor->HasPuzzle())
	{
		ShowMessage(TEXT("Nothing to save."), true);
		return;
	}
	const FString File = UGatherAndHuntFrontEnd::NormalizePuzzleFilename(FileBox->GetText().ToString());
	if (File.IsEmpty())
	{
		ShowMessage(TEXT("Type a file name first, e.g. Act1/Crossing."), true);
		return;
	}

	// Writing over a different puzzle's file takes a second press.
	UWorld* const World = GetWorld();
	AGatherAndHuntPuzzleGameMode* const GameMode = World != nullptr ? World->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>() : nullptr;
	const FString LoadedFile = GameMode != nullptr ? UGatherAndHuntFrontEnd::NormalizePuzzleFilename(GameMode->AutoLoadPuzzleRelativeFilename) : FString();
	if (File != LoadedFile && UGatherAndHuntFrontEnd::PuzzleFileExists(File) && PendingOverwriteFile != File)
	{
		PendingOverwriteFile = File;
		ShowMessage(FString::Printf(TEXT("%s already exists. Press Save again to overwrite it."), *File), true);
		return;
	}
	PendingOverwriteFile.Reset();

	if (!Session->SavePuzzleToProjectFile(File))
	{
		ShowMessage(FString::Printf(TEXT("Could not save %s."), *File), true);
		return;
	}
	TargetFile = File;
	if (GameMode != nullptr)
	{
		// The planner's Ctrl+S and Reset follow the file now being edited.
		GameMode->AutoLoadPuzzleRelativeFilename = File;
	}

	int32 ErrorCount = 0;
	for (const FLevelEditorValidationIssue& Issue : Editor->GetIssues())
	{
		ErrorCount += Issue.bIsError ? 1 : 0;
	}
	ShowMessage(ErrorCount == 0
		? FString::Printf(TEXT("Saved %s."), *File)
		: FString::Printf(TEXT("Saved %s, with %d error(s) to fix before it's playable."), *File, ErrorCount),
		ErrorCount > 0);
	Refresh();
}

void UGatherAndHuntBoardEditorWidget::NewPuzzle()
{
	if (Editor == nullptr || !ConfirmDiscard(static_cast<int32>(EAction::New), TEXT("New")))
	{
		return;
	}
	Editor->NewPuzzle(TEXT("New puzzle"), DefaultNewBoardSize);
	if (AGatherAndHuntPuzzleGameMode* const GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>() : nullptr)
	{
		// Not backed by a file until saved; keeps Ctrl+S from hitting the old one.
		GameMode->AutoLoadPuzzleRelativeFilename.Reset();
	}
	SetTargetFile(FString());
	ShowMessage(TEXT("New board. Place a turtle (1), name the file, then save."), false);
	Refresh();
}

void UGatherAndHuntBoardEditorWidget::BackToPlanner()
{
	if (AGatherAndHuntPuzzleHUD* const HUD = GetOwningPlayer() != nullptr ? GetOwningPlayer()->GetHUD<AGatherAndHuntPuzzleHUD>() : nullptr)
	{
		HUD->SetBoardEditing(false);
	}
}

void UGatherAndHuntBoardEditorWidget::BackToMenu()
{
	if (ConfirmDiscard(static_cast<int32>(EAction::Menu), TEXT("Main menu")))
	{
		UGatherAndHuntFrontEnd::OpenMainMenu(this);
	}
}

// --- Text boxes -----------------------------------------------------------------

void UGatherAndHuntBoardEditorWidget::HandleNameCommitted(const FText& Text, const ETextCommit::Type CommitType)
{
	if (Editor != nullptr && Editor->HasPuzzle() && CommitType != ETextCommit::OnCleared)
	{
		Editor->SetPuzzleName(Text.ToString());
	}
	if (CommitType == ETextCommit::OnEnter)
	{
		SetKeyboardFocus();
	}
}

void UGatherAndHuntBoardEditorWidget::HandleFileCommitted(const FText& Text, const ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnCleared)
	{
		SetTargetFile(Text.ToString());
	}
	if (CommitType == ETextCommit::OnEnter)
	{
		SetKeyboardFocus();
	}
}

void UGatherAndHuntBoardEditorWidget::HandleIntroCommitted(const FText& Text, const ETextCommit::Type CommitType)
{
	// Multi-line: Enter makes a new line, so this commits when focus leaves.
	if (Editor != nullptr && Editor->HasPuzzle() && CommitType != ETextCommit::OnCleared)
	{
		Editor->SetIntroText(Text.ToString());
	}
}

void UGatherAndHuntBoardEditorWidget::HandleEditorChanged(const bool bBoardChanged)
{
	Refresh();
}

// --- Input ----------------------------------------------------------------------

FIntVector UGatherAndHuntBoardEditorWidget::CameraRelativeStep(const int32 ScreenDirection) const
{
	// The camera looks along its arm's yaw; W steps along whichever grid axis
	// is closest to that, and A/S/D are that axis turned by quarters.
	const float Yaw = (BoardView != nullptr ? BoardView->GetCameraYaw() : 0.0f) + ScreenDirection * 90.0f;
	const float X = FMath::Cos(FMath::DegreesToRadians(Yaw));
	const float Y = FMath::Sin(FMath::DegreesToRadians(Yaw));
	return FMath::Abs(X) >= FMath::Abs(Y)
		? FIntVector(X >= 0.0f ? 1 : -1, 0, 0)
		: FIntVector(0, Y >= 0.0f ? 1 : -1, 0);
}

void UGatherAndHuntBoardEditorWidget::Step(const FIntVector& Delta, const bool bCarryPiece)
{
	if (Editor == nullptr)
	{
		return;
	}
	FGatherAndHuntBoardEditorPiece Piece;
	if (bCarryPiece && Editor->GetSelectedPiece(Piece))
	{
		if (!Editor->MoveSelectedPiece(Delta))
		{
			ShowMessage(TEXT("Can't move there: off the board or occupied."), true);
		}
		return;
	}
	Editor->MoveCursor(Delta);
}

void UGatherAndHuntBoardEditorWidget::PickCellAt(const FVector2D& ScreenPosition)
{
	APlayerController* const PC = GetOwningPlayer();
	if (PC == nullptr || BoardView == nullptr || Editor == nullptr)
	{
		return;
	}
	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(this, ScreenPosition, PixelPosition, ViewportPosition);
	FVector Origin;
	FVector Direction;
	FIntVector Cell;
	if (PC->DeprojectScreenPositionToWorld(PixelPosition.X, PixelPosition.Y, Origin, Direction)
		&& BoardView->PickEditorCell(Origin, Direction, Cell))
	{
		Editor->SetCursor(Cell);
	}
}

FReply UGatherAndHuntBoardEditorWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (Editor == nullptr)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	const FKey Key = InKeyEvent.GetKey();
	const bool bCtrl = InKeyEvent.IsControlDown();
	const bool bShift = InKeyEvent.IsShiftDown();
	auto Do = [this](const EAction Action) { OnAction(static_cast<int32>(Action)); };

	if (bCtrl)
	{
		if (Key == EKeys::S) { Do(EAction::Save); }
		else if (Key == EKeys::Z) { Do(bShift ? EAction::Redo : EAction::Undo); }
		else if (Key == EKeys::Y) { Do(EAction::Redo); }
		else { return Super::NativeOnKeyDown(InGeometry, InKeyEvent); }
		return FReply::Handled();
	}

	struct FStepKey { FKey Key; int32 ScreenDirection; };
	static const FStepKey StepKeys[] = {
		{EKeys::W, 0}, {EKeys::Up, 0}, {EKeys::D, 1}, {EKeys::Right, 1},
		{EKeys::S, 2}, {EKeys::Down, 2}, {EKeys::A, 3}, {EKeys::Left, 3}};
	for (const FStepKey& StepKey : StepKeys)
	{
		if (Key == StepKey.Key)
		{
			Step(CameraRelativeStep(StepKey.ScreenDirection), bShift);
			return FReply::Handled();
		}
	}

	static const FKey DigitKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(DigitKeys); ++Index)
	{
		if (Key == DigitKeys[Index])
		{
			OnPaletteButton(static_cast<int32>(Palette[Index].Kind));
			return FReply::Handled();
		}
	}

	if (Key == EKeys::E) { Step(FIntVector(0, 0, 1), bShift); }
	else if (Key == EKeys::Q) { Step(FIntVector(0, 0, -1), bShift); }
	else if (Key == EKeys::Delete || Key == EKeys::BackSpace) { Do(EAction::Delete); }
	else if (Key == EKeys::R) { Do(bShift ? EAction::RotateLeft : EAction::RotateRight); }
	else if (Key == EKeys::T) { Do(EAction::PitchUp); }
	else if (Key == EKeys::G) { Do(EAction::PitchDown); }
	else if (Key == EKeys::C) { Editor->CycleSelectedTurtleRole(); }
	else if (Key == EKeys::B) { Do(EAction::Planner); }
	else if (Key == EKeys::Home) { Do(EAction::Frame); }
	else { return Super::NativeOnKeyDown(InGeometry, InKeyEvent); }
	return FReply::Handled();
}

FReply UGatherAndHuntBoardEditorWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Reaches here only for presses no button or text box handled: empty space.
	bPointerDown = true;
	bOrbiting = false;
	PressScreenPosition = InMouseEvent.GetScreenSpacePosition();
	return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget());
}

FReply UGatherAndHuntBoardEditorWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bPointerDown)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}
	bPointerDown = false;
	if (!bOrbiting && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		PickCellAt(InMouseEvent.GetScreenSpacePosition());
	}
	bOrbiting = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UGatherAndHuntBoardEditorWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bPointerDown && BoardView != nullptr)
	{
		if (!bOrbiting && FVector2D::Distance(InMouseEvent.GetScreenSpacePosition(), PressScreenPosition) >= ClickThreshold)
		{
			bOrbiting = true;
		}
		if (bOrbiting)
		{
			const FVector2D Delta = InMouseEvent.GetCursorDelta();
			BoardView->OrbitCamera(Delta.X * 0.3f, -Delta.Y * 0.3f);
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UGatherAndHuntBoardEditorWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (BoardView != nullptr)
	{
		BoardView->ZoomCamera(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}
