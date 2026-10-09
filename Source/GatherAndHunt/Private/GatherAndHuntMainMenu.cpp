#include "GatherAndHuntMainMenu.h"

#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "GameFramework/PlayerController.h"
#include "GatherAndHuntPlayerOptions.h"
#include "GatherAndHuntWidgetBuilder.h"
#include "Kismet/KismetSystemLibrary.h"

using namespace GatherAndHuntUI;

namespace
{
	enum class EMenuAction : int32
	{
		Play,
		Edit,
		New,
		Rescan,
		Campaign,
		ArtPreview,
		Options,
		Quit
	};

	const FLinearColor BackdropColor(0.015f, 0.02f, 0.03f, 1.0f);
}

UGatherAndHuntMainMenuWidget::UGatherAndHuntMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UGatherAndHuntMainMenuWidget::RebuildWidget()
{
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
		RefreshPuzzles();
	}
	return Super::RebuildWidget();
}

void UGatherAndHuntMainMenuWidget::BuildTree()
{
	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(BackdropColor);
	Backdrop->SetPadding(FMargin(0.0f));
	WidgetTree->RootWidget = Backdrop;

	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Backdrop->SetContent(Canvas);

	auto Action = [this](const FString& Label, const EMenuAction InAction, const int32 FontSize = 12)
	{
		return MakeButton(WidgetTree, this, Label, static_cast<int32>(InAction), &UGatherAndHuntMainMenuWidget::OnAction, FontSize);
	};

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	AddRow(Column, MakeText(WidgetTree, TEXT("GATHER AND HUNT"), 32), 0.0f);
	AddRow(Column, MakeText(WidgetTree, TEXT("Pick a puzzle to play or edit."), 12, DimTextColor), 4.0f);

	UHorizontalBox* const Body = WidgetTree->ConstructWidget<UHorizontalBox>();

	// Left: the puzzle list.
	{
		UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBox* const Header = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Header, MakeText(WidgetTree, TEXT("PUZZLES"), 10, DimTextColor), 12.0f);
		AddCell(Header, Action(TEXT("Rescan"), EMenuAction::Rescan, 10));
		AddRow(Box, Header, 0.0f);
		PuzzleList = WidgetTree->ConstructWidget<UScrollBox>();
		AddRow(Box, SizeTo(WidgetTree, PuzzleList, 420.0f, 440.0f), 6.0f);
		AddCell(Body, MakePanel(WidgetTree, Box), 16.0f)->SetVerticalAlignment(VAlign_Top);
	}

	// Right: details, actions, new puzzle.
	{
		UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
		DetailTitle = MakeText(WidgetTree, TEXT(""), 18);
		AddRow(Box, DetailTitle, 0.0f);
		DetailText = MakeText(WidgetTree, TEXT(""), 11, DimTextColor);
		DetailText->SetAutoWrapText(true);
		AddRow(Box, SizeTo(WidgetTree, DetailText, 400.0f), 6.0f);

		UHorizontalBox* const Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
		PlayButton = Action(TEXT("  PLAY (Enter)  "), EMenuAction::Play, 14);
		EditButton = Action(TEXT("  Edit board  "), EMenuAction::Edit, 14);
		AddCell(Buttons, PlayButton, 8.0f);
		AddCell(Buttons, EditButton);
		AddRow(Box, Buttons, 14.0f);

		AddRow(Box, MakeText(WidgetTree, TEXT("NEW PUZZLE"), 10, DimTextColor), 28.0f);
		UHorizontalBox* const NewRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		NewFileBox = WidgetTree->ConstructWidget<UEditableTextBox>();
		NewFileBox->SetHintText(FText::FromString(TEXT("File name, e.g. Act1/Crossing")));
		SetFontSize(NewFileBox, 11);
		AddCell(NewRow, SizeTo(WidgetTree, NewFileBox, 260.0f));
		AddCell(NewRow, Action(TEXT("Create"), EMenuAction::New));
		AddRow(Box, NewRow, 4.0f);

		MessageText = MakeText(WidgetTree, TEXT(""), 10, GoodColor);
		MessageText->SetAutoWrapText(true);
		AddRow(Box, SizeTo(WidgetTree, MessageText, 400.0f), 6.0f);

		UHorizontalBox* const Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Footer, Action(TEXT("Level select (Y)"), EMenuAction::Campaign), 8.0f);
		AddCell(Footer, Action(TEXT("Art preview"), EMenuAction::ArtPreview), 8.0f);
		AddCell(Footer, Action(TEXT("Options"), EMenuAction::Options), 8.0f);
		AddCell(Footer, Action(TEXT("Quit"), EMenuAction::Quit));
		AddRow(Box, Footer, 28.0f);
		AddCell(Body, SizeTo(WidgetTree, MakePanel(WidgetTree, Box), 440.0f))->SetVerticalAlignment(VAlign_Top);
	}

	AddRow(Column, Body, 20.0f);
	Place(Canvas, Column, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	OptionsPanel = WidgetTree->ConstructWidget<UGatherAndHuntOptionsWidget>();
	OptionsPanel->SetVisibility(ESlateVisibility::Collapsed);
	Place(Canvas, OptionsPanel, FVector2D(1.0f, 0.0f), FVector2D(-16.0f, 16.0f));
}

void UGatherAndHuntMainMenuWidget::RefreshPuzzles()
{
	const FString Previous = Puzzles.IsValidIndex(SelectedIndex) ? Puzzles[SelectedIndex].RelativeFilename : FString();
	Puzzles = UGatherAndHuntFrontEnd::ListPuzzles();
	SelectedIndex = Puzzles.Num() > 0 ? 0 : INDEX_NONE;
	for (int32 Index = 0; Index < Puzzles.Num(); ++Index)
	{
		if (Puzzles[Index].RelativeFilename == Previous)
		{
			SelectedIndex = Index;
		}
	}

	if (PuzzleList == nullptr)
	{
		return;
	}
	PuzzleList->ClearChildren();
	PuzzleButtons.Reset();
	int32 CurrentAct = INDEX_NONE;
	for (int32 Index = 0; Index < Puzzles.Num(); ++Index)
	{
		const FGatherAndHuntPuzzleListing& Puzzle = Puzzles[Index];
		if (Puzzle.Act != CurrentAct)
		{
			CurrentAct = Puzzle.Act;
			PuzzleList->AddChild(MakeText(WidgetTree, FString::Printf(TEXT("ACT %d"), CurrentAct), 10, DimTextColor));
		}
		UGatherAndHuntPlannerButton* const Button = MakeButton(WidgetTree, this,
			FString::Printf(TEXT("%s\n%s"), *Puzzle.PuzzleName, *Puzzle.RelativeFilename),
			Index, &UGatherAndHuntMainMenuWidget::OnPuzzle, 11);
		AlignContentLeft(Button);
		PuzzleList->AddChild(Button);
		PuzzleButtons.Add(Button);
	}
	if (Puzzles.Num() == 0)
	{
		PuzzleList->AddChild(MakeText(WidgetTree, TEXT("No board files in Content/BoardSetups yet."), 11, DimTextColor));
	}
	RefreshDetails();
}

void UGatherAndHuntMainMenuWidget::RefreshDetails()
{
	for (int32 Index = 0; Index < PuzzleButtons.Num(); ++Index)
	{
		PuzzleButtons[Index]->SetBackgroundColor(Index == SelectedIndex ? SelectedColor : FLinearColor::White);
	}

	const bool bHasSelection = Puzzles.IsValidIndex(SelectedIndex);
	PlayButton->SetIsEnabled(bHasSelection && Puzzles[SelectedIndex].bLoaded);
	EditButton->SetIsEnabled(bHasSelection && Puzzles[SelectedIndex].bLoaded);
	if (!bHasSelection)
	{
		DetailTitle->SetText(FText::FromString(TEXT("No puzzle selected")));
		DetailText->SetText(FText::GetEmpty());
		return;
	}

	const FGatherAndHuntPuzzleListing& Puzzle = Puzzles[SelectedIndex];
	DetailTitle->SetText(FText::FromString(Puzzle.PuzzleName));
	FString Details = FString::Printf(TEXT("%s    Act %d, #%d"), *Puzzle.RelativeFilename, Puzzle.Act, Puzzle.Order);
	if (!Puzzle.bLoaded)
	{
		Details += TEXT("\n\nThis file could not be read.");
	}
	else
	{
		if (Puzzle.TargetScore > 0 || Puzzle.TargetTurnCount > 0)
		{
			Details += TEXT("\n");
			if (Puzzle.TargetScore > 0) Details += FString::Printf(TEXT("Score %d   "), Puzzle.TargetScore);
			if (Puzzle.TargetTurnCount > 0) Details += FString::Printf(TEXT("Within %d turns"), Puzzle.TargetTurnCount);
		}
		if (!Puzzle.IntroText.IsEmpty())
		{
			Details += TEXT("\n\n") + Puzzle.IntroText;
		}
	}
	DetailText->SetText(FText::FromString(Details));
}

void UGatherAndHuntMainMenuWidget::OnPuzzle(const int32 Index)
{
	SelectedIndex = Index;
	RefreshDetails();
}

void UGatherAndHuntMainMenuWidget::OnAction(const int32 InAction)
{
	switch (static_cast<EMenuAction>(InAction))
	{
	case EMenuAction::Play:
	case EMenuAction::Edit:
		if (Puzzles.IsValidIndex(SelectedIndex))
		{
			UGatherAndHuntFrontEnd::OpenPuzzle(this, Puzzles[SelectedIndex].RelativeFilename, static_cast<EMenuAction>(InAction) == EMenuAction::Edit);
		}
		break;
	case EMenuAction::New:
	{
		const FString File = UGatherAndHuntFrontEnd::NormalizePuzzleFilename(NewFileBox->GetText().ToString());
		if (File.IsEmpty())
		{
			ShowMessage(TEXT("Type a file name for the new puzzle."), true);
		}
		else if (UGatherAndHuntFrontEnd::PuzzleFileExists(File))
		{
			ShowMessage(FString::Printf(TEXT("%s already exists. Select it and use Edit board."), *File), true);
		}
		else
		{
			UGatherAndHuntFrontEnd::OpenPuzzle(this, File, true);
		}
		break;
	}
	case EMenuAction::Rescan:
		RefreshPuzzles();
		ShowMessage(FString::Printf(TEXT("%d puzzle(s) found."), Puzzles.Num()), false);
		break;
	case EMenuAction::Campaign:
		UGatherAndHuntFrontEnd::OpenCampaignMap(this);
		break;
	case EMenuAction::ArtPreview:
		UGatherAndHuntFrontEnd::OpenArtPreview(this);
		break;
	case EMenuAction::Options:
		OptionsPanel->SetVisibility(OptionsPanel->IsVisible() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		break;
	case EMenuAction::Quit:
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		break;
	}
}

void UGatherAndHuntMainMenuWidget::ShowMessage(const FString& Message, const bool bIsError)
{
	MessageText->SetText(FText::FromString(Message));
	MessageText->SetColorAndOpacity(FSlateColor(bIsError ? ErrorColor : GoodColor));
}

FReply UGatherAndHuntMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bDown = Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down;
	if ((Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || bDown) && Puzzles.Num() > 0)
	{
		SelectedIndex = FMath::Clamp(SelectedIndex + (bDown ? 1 : -1), 0, Puzzles.Num() - 1);
		RefreshDetails();
		if (PuzzleButtons.IsValidIndex(SelectedIndex))
		{
			PuzzleList->ScrollWidgetIntoView(PuzzleButtons[SelectedIndex]);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		OnAction(static_cast<int32>(EMenuAction::Play));
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Top)
	{
		OnAction(static_cast<int32>(EMenuAction::Campaign));
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// --- HUD and GameMode -------------------------------------------------------------

AGatherAndHuntMenuHUD::AGatherAndHuntMenuHUD()
{
	MenuWidgetClass = UGatherAndHuntMainMenuWidget::StaticClass();
}

void AGatherAndHuntMenuHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* const PC = GetOwningPlayerController();
	if (PC == nullptr)
	{
		return;
	}
	MenuWidget = CreateWidget<UGatherAndHuntMainMenuWidget>(
		PC,
		MenuWidgetClass != nullptr ? MenuWidgetClass.Get() : UGatherAndHuntMainMenuWidget::StaticClass());
	MenuWidget->AddToViewport();
	PC->SetShowMouseCursor(true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
}

AGatherAndHuntMenuGameMode::AGatherAndHuntMenuGameMode()
{
	HUDClass = AGatherAndHuntMenuHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
