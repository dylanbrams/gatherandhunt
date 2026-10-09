#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GatherAndHuntInput.h"
#include "GatherAndHuntPlayerOptions.generated.h"

class UTextBlock;

// What a player can tune, saved per user (Saved/Config/.../GameUserSettings.ini).
// The far lights are the dots that show a ship's heading from a distance; how
// they should look depends on the board's size and the player's screen.
UCLASS(Config = GameUserSettings)
class GATHERANDHUNT_API UGatherAndHuntPlayerOptions : public UObject
{
	GENERATED_BODY()

public:
	// The one instance; change it and call Save.
	static UGatherAndHuntPlayerOptions* Get();

	void Save();
	void ResetToDefaults();

	// Multiplies the far lights' minimum on-screen size.
	UPROPERTY(Config)
	float FarLightSize = 1.0f;

	// Multiplies the camera distance at which the far lights come in: above 1
	// they wait until the camera is further away.
	UPROPERTY(Config)
	float FarLightDistance = 1.0f;

	// Multiplies how fast the far lights pulse and blink.
	UPROPERTY(Config)
	float FarLightBlinkSpeed = 1.0f;

	// The player's own controls: changes to rows of UGatherAndHuntInput's
	// bindings table, by row id. Call UGatherAndHuntInput::RebuildMappings on a
	// live screen after changing them.
	UPROPERTY(Config)
	TArray<FGatherAndHuntKeyOverride> KeyOverrides;

	// Every option is a multiplier kept within this range.
	static constexpr float MinMultiplier = 0.2f;
	static constexpr float MaxMultiplier = 5.0f;
};

// A panel of steppers for UGatherAndHuntPlayerOptions. Changes apply at once
// and are saved. Any screen that builds its widget tree in C++ can add one.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntOptionsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildTree();
	void Refresh();
	void OnAction(int32 Action);

	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ValueTexts;
};
