#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GatherAndHuntInput.h"
#include "GatherAndHuntPlayerOptions.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntInputBindingsTest,
	"GatherAndHunt.GatherAndHunt.Input.BindingsTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntInputBindingsTest::RunTest(const FString&)
{
	using EAction = EGatherAndHuntInputAction;
	const TArray<FGatherAndHuntInputBinding>& Defaults = UGatherAndHuntInput::GetDefaultBindings();

	// Overrides refer to rows by id, so ids can't repeat.
	TSet<FName> Ids;
	for (const FGatherAndHuntInputBinding& Binding : Defaults)
	{
		bool bDuplicate = false;
		Ids.Add(Binding.Id, &bDuplicate);
		TestFalse(*FString::Printf(TEXT("Binding id %s is used once"), *Binding.Id.ToString()), bDuplicate);
	}

	auto FindKeys = [&Defaults](const EAction Action, const bool bGamepad)
	{
		TArray<FKey> Keys;
		for (const FGatherAndHuntInputBinding& Binding : Defaults)
		{
			if (Binding.Action == Action && Binding.Key.IsGamepadKey() == bGamepad)
			{
				Keys.Add(Binding.Key);
			}
		}
		return Keys;
	};

	// Playing a level needs nothing but the gamepad, and nothing but the keyboard.
	const UEnum* const ActionEnum = StaticEnum<EGatherAndHuntInputAction>();
	for (const EAction Action : {EAction::MoveForward, EAction::MoveYawLeft, EAction::MoveYawRight, EAction::MovePitchUp, EAction::MovePitchDown, EAction::MoveWait,
		EAction::Undo, EAction::Submit, EAction::Back, EAction::ResetPuzzle, EAction::NextShip, EAction::PreviousShip, EAction::CameraFly, EAction::ShowNames})
	{
		const FString Name = ActionEnum->GetNameStringByValue(static_cast<int64>(Action));
		TestTrue(*FString::Printf(TEXT("%s has a gamepad binding"), *Name), FindKeys(Action, true).Num() > 0);
		TestTrue(*FString::Printf(TEXT("%s has a keyboard binding"), *Name), FindKeys(Action, false).Num() > 0);
	}

	TestTrue(TEXT("] pages to the next ship"), FindKeys(EAction::NextShip, false).Contains(EKeys::RightBracket));
	TestTrue(TEXT("[ pages to the previous ship"), FindKeys(EAction::PreviousShip, false).Contains(EKeys::LeftBracket));

	// A player's override replaces one row's key and leaves the rest alone.
	UGatherAndHuntPlayerOptions* const Options = UGatherAndHuntPlayerOptions::Get();
	const TArray<FGatherAndHuntKeyOverride> Saved = Options->KeyOverrides;
	Options->KeyOverrides.Reset();
	FGatherAndHuntKeyOverride& Override = Options->KeyOverrides.AddDefaulted_GetRef();
	Override.Id = TEXT("Ship.Forward");
	Override.Key = EKeys::I;
	int32 Changed = 0;
	const TArray<FGatherAndHuntInputBinding> Custom = UGatherAndHuntInput::GetBindings();
	for (int32 Index = 0; Index < Custom.Num(); ++Index)
	{
		Changed += Custom[Index].Key != Defaults[Index].Key ? 1 : 0;
		if (Custom[Index].Id == Override.Id)
		{
			TestEqual(TEXT("The overridden row uses the player's key"), Custom[Index].Key, EKeys::I);
		}
	}
	TestEqual(TEXT("Only the overridden row changed"), Changed, 1);
	Options->KeyOverrides = Saved;
	return true;
}

#endif
