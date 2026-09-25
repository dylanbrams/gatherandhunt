#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GatherAndHuntPlannerWidget.h"

// Shared look and tree-building helpers for the screens that build their UMG
// trees in C++ (board editor, main menu). Matches the planner's style.
namespace GatherAndHuntUI
{
	const FLinearColor PanelColor(0.02f, 0.02f, 0.03f, 0.78f);
	const FLinearColor DimTextColor(0.65f, 0.65f, 0.7f);
	const FLinearColor ErrorColor(1.0f, 0.3f, 0.25f);
	const FLinearColor WarningColor(1.0f, 0.7f, 0.1f);
	const FLinearColor GoodColor(0.35f, 1.0f, 0.45f);
	const FLinearColor SelectedColor(0.0f, 0.55f, 0.75f);

	inline UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, const int32 Size = 11, const FLinearColor& Color = FLinearColor::White)
	{
		UTextBlock* const Block = Tree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetText(FText::FromString(Text));
		return Block;
	}

	// A non-focusable button that calls Handler(Payload) on the owner.
	template <typename OwnerType>
	UGatherAndHuntPlannerButton* MakeButton(UWidgetTree* Tree, OwnerType* Owner, const FString& Label, const int32 Payload, void (OwnerType::*Handler)(int32), const int32 FontSize = 11)
	{
		UGatherAndHuntPlannerButton* const Button = Tree->ConstructWidget<UGatherAndHuntPlannerButton>();
		Button->Payload = Payload;
		Button->OnIndexedClick.BindUObject(Owner, Handler);
		Button->OnClicked.AddDynamic(Button, &UGatherAndHuntPlannerButton::HandleClicked);
		Button->SetContent(MakeText(Tree, Label, FontSize));
		return Button;
	}

	inline UBorder* MakePanel(UWidgetTree* Tree, UWidget* Content)
	{
		UBorder* const Panel = Tree->ConstructWidget<UBorder>();
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(10.0f));
		Panel->SetContent(Content);
		return Panel;
	}

	inline UWidget* SizeTo(UWidgetTree* Tree, UWidget* Content, const float Width, const float Height = 0.0f)
	{
		USizeBox* const Box = Tree->ConstructWidget<USizeBox>();
		if (Width > 0.0f) Box->SetWidthOverride(Width);
		if (Height > 0.0f) Box->SetHeightOverride(Height);
		Box->SetContent(Content);
		return Box;
	}

	inline void AddRow(UVerticalBox* Box, UWidget* Widget, const float Top = 2.0f)
	{
		Box->AddChildToVerticalBox(Widget)->SetPadding(FMargin(0.0f, Top, 0.0f, 0.0f));
	}

	inline UHorizontalBoxSlot* AddCell(UHorizontalBox* Box, UWidget* Widget, const float Right = 4.0f)
	{
		UHorizontalBoxSlot* const CellSlot = Box->AddChildToHorizontalBox(Widget);
		CellSlot->SetPadding(FMargin(0.0f, 0.0f, Right, 0.0f));
		CellSlot->SetVerticalAlignment(VAlign_Center);
		return CellSlot;
	}

	inline void AlignContentLeft(UButton* Button)
	{
		if (UButtonSlot* const ContentSlot = Cast<UButtonSlot>(Button->GetContentSlot()))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Left);
		}
	}

	// Text boxes default to a large font and light text on a light box; match
	// the panels and make the text readable.
	inline void StyleTextBox(FEditableTextBoxStyle& Style, const int32 FontSize)
	{
		const FSlateColor Ink(FLinearColor(0.02f, 0.02f, 0.03f));
		Style.TextStyle.Font.Size = FontSize;
		Style.ForegroundColor = Ink;
		Style.FocusedForegroundColor = Ink;
	}

	inline void SetFontSize(UEditableTextBox* Box, const int32 Size)
	{
		FEditableTextBoxStyle Style = Box->GetWidgetStyle();
		StyleTextBox(Style, Size);
		Box->SetWidgetStyle(Style);
	}

	inline void SetFontSize(UMultiLineEditableTextBox* Box, const int32 Size)
	{
		StyleTextBox(Box->WidgetStyle, Size);
	}

	// Anchor doubles as alignment, so (1, 0) pins the widget's top-right corner
	// to the canvas's top-right plus Offset.
	inline void Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D Anchor, const FVector2D Offset)
	{
		UCanvasPanelSlot* const CanvasSlot = Canvas->AddChildToCanvas(Widget);
		CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		CanvasSlot->SetAlignment(Anchor);
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetPosition(Offset);
	}
}
