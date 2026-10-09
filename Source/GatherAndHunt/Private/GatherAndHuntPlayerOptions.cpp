#include "GatherAndHuntPlayerOptions.h"

#include "GatherAndHuntWidgetBuilder.h"

using namespace GatherAndHuntUI;

namespace
{
	constexpr float MultiplierStep = 0.1f;

	// An option with -/+ buttons.
	struct FOptionRow
	{
		const TCHAR* Label;
		float UGatherAndHuntPlayerOptions::* Value;
	};

	const FOptionRow OptionRows[] = {
		{TEXT("Size"), &UGatherAndHuntPlayerOptions::FarLightSize},
		{TEXT("Appear at distance"), &UGatherAndHuntPlayerOptions::FarLightDistance},
		{TEXT("Blink speed"), &UGatherAndHuntPlayerOptions::FarLightBlinkSpeed},
	};

	// Action payloads: two per row (down, up), then this.
	constexpr int32 ActionReset = 100;
}

UGatherAndHuntPlayerOptions* UGatherAndHuntPlayerOptions::Get()
{
	return GetMutableDefault<UGatherAndHuntPlayerOptions>();
}

void UGatherAndHuntPlayerOptions::Save()
{
	SaveConfig();
}

void UGatherAndHuntPlayerOptions::ResetToDefaults()
{
	FarLightSize = 1.0f;
	FarLightDistance = 1.0f;
	FarLightBlinkSpeed = 1.0f;
}

TSharedRef<SWidget> UGatherAndHuntOptionsWidget::RebuildWidget()
{
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
		Refresh();
	}
	return Super::RebuildWidget();
}

void UGatherAndHuntOptionsWidget::BuildTree()
{
	UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
	AddRow(Box, MakeText(WidgetTree, TEXT("FAR LIGHTS"), 10, DimTextColor), 0.0f);
	UTextBlock* const Hint = MakeText(WidgetTree, TEXT("The dots that show a ship's heading when zoomed out. Saved as you change them."), 10, DimTextColor);
	Hint->SetAutoWrapText(true);
	AddRow(Box, Hint);

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(OptionRows); ++Index)
	{
		UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Row, MakeButton(WidgetTree, this, TEXT(" - "), Index * 2, &UGatherAndHuntOptionsWidget::OnAction));
		AddCell(Row, MakeButton(WidgetTree, this, TEXT(" + "), Index * 2 + 1, &UGatherAndHuntOptionsWidget::OnAction), 8.0f);
		UTextBlock* const Text = MakeText(WidgetTree, TEXT(""));
		AddCell(Row, Text);
		ValueTexts.Add(Text);
		AddRow(Box, Row, Index == 0 ? 8.0f : 2.0f);
	}
	AddRow(Box, MakeButton(WidgetTree, this, TEXT("Reset"), ActionReset, &UGatherAndHuntOptionsWidget::OnAction), 8.0f);

	WidgetTree->RootWidget = SizeTo(WidgetTree, MakePanel(WidgetTree, Box), 280.0f);
}

void UGatherAndHuntOptionsWidget::Refresh()
{
	const UGatherAndHuntPlayerOptions* const Options = UGatherAndHuntPlayerOptions::Get();
	for (int32 Index = 0; Index < ValueTexts.Num(); ++Index)
	{
		ValueTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s  x%.1f"), OptionRows[Index].Label, Options->*OptionRows[Index].Value)));
	}
}

void UGatherAndHuntOptionsWidget::OnAction(const int32 InAction)
{
	UGatherAndHuntPlayerOptions* const Options = UGatherAndHuntPlayerOptions::Get();
	if (InAction == ActionReset)
	{
		Options->ResetToDefaults();
	}
	else if (InAction >= 0 && InAction < 2 * static_cast<int32>(UE_ARRAY_COUNT(OptionRows)))
	{
		float& Value = Options->*OptionRows[InAction / 2].Value;
		Value = FMath::Clamp(
			Value + (InAction % 2 == 0 ? -MultiplierStep : MultiplierStep),
			UGatherAndHuntPlayerOptions::MinMultiplier,
			UGatherAndHuntPlayerOptions::MaxMultiplier);
	}
	Options->Save();
	Refresh();
}
