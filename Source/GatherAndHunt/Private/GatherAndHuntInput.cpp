#include "GatherAndHuntInput.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "GatherAndHuntPlayerOptions.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"

namespace
{
	using EAction = EGatherAndHuntInputAction;
	using EContext = EGatherAndHuntInputContext;
	using EShape = EGatherAndHuntInputShape;

	constexpr int32 ActionCount = static_cast<int32>(EAction::Count);
	constexpr int32 ContextCount = static_cast<int32>(EContext::Count);

	// Sticks rest a little off centre.
	constexpr float StickDeadZone = 0.2f;

	EInputActionValueType ValueTypeOf(const EAction Action)
	{
		switch (Action)
		{
		case EAction::CameraFly: return EInputActionValueType::Axis3D;
		case EAction::CameraTurn: return EInputActionValueType::Axis2D;
		case EAction::SelectShip: return EInputActionValueType::Axis1D;
		default: return EInputActionValueType::Boolean;
		}
	}

	// Higher wins where it matters; combinations are looked at first.
	int32 PriorityOf(const EContext Context)
	{
		switch (Context)
		{
		case EContext::Chords: return 2;
		case EContext::CameraKeys:
		case EContext::ShipKeys: return 1;
		default: return 0;
		}
	}

	TArray<FGatherAndHuntInputBinding> MakeDefaultBindings()
	{
		TArray<FGatherAndHuntInputBinding> Bindings;
		auto Add = [&Bindings](const TCHAR* Id, const EAction Action, const FKey& Key, const EContext Context = EContext::Common,
			const EShape Shape = EShape::Plain, const float Scale = 1.0f, const EAction Chord = EAction::Count)
		{
			FGatherAndHuntInputBinding& Binding = Bindings.AddDefaulted_GetRef();
			Binding.Id = Id;
			Binding.Action = Action;
			Binding.Key = Key;
			Binding.Context = Context;
			Binding.Shape = Shape;
			Binding.Scale = Scale;
			Binding.Chord = Chord;
		};

		// --- Letter keys while they move the ship (after Tab) ---
		Add(TEXT("Ship.Forward"), EAction::MoveForward, EKeys::W, EContext::ShipKeys);
		Add(TEXT("Ship.ForwardAlt"), EAction::MoveForward, EKeys::Up, EContext::ShipKeys);
		Add(TEXT("Ship.YawLeft"), EAction::MoveYawLeft, EKeys::A, EContext::ShipKeys);
		Add(TEXT("Ship.YawLeftAlt"), EAction::MoveYawLeft, EKeys::Left, EContext::ShipKeys);
		Add(TEXT("Ship.YawRight"), EAction::MoveYawRight, EKeys::D, EContext::ShipKeys);
		Add(TEXT("Ship.YawRightAlt"), EAction::MoveYawRight, EKeys::Right, EContext::ShipKeys);
		Add(TEXT("Ship.PitchUp"), EAction::MovePitchUp, EKeys::R, EContext::ShipKeys);
		Add(TEXT("Ship.PitchDown"), EAction::MovePitchDown, EKeys::F, EContext::ShipKeys);
		Add(TEXT("Ship.Wait"), EAction::MoveWait, EKeys::S, EContext::ShipKeys);
		Add(TEXT("Ship.WaitAlt"), EAction::MoveWait, EKeys::Down, EContext::ShipKeys);

		// --- Letter keys while they fly the camera (the default) ---
		Add(TEXT("Camera.Forward"), EAction::CameraFly, EKeys::W, EContext::CameraKeys);
		Add(TEXT("Camera.ForwardAlt"), EAction::CameraFly, EKeys::Up, EContext::CameraKeys);
		Add(TEXT("Camera.Back"), EAction::CameraFly, EKeys::S, EContext::CameraKeys, EShape::Negative);
		Add(TEXT("Camera.BackAlt"), EAction::CameraFly, EKeys::Down, EContext::CameraKeys, EShape::Negative);
		Add(TEXT("Camera.Right"), EAction::CameraFly, EKeys::D, EContext::CameraKeys, EShape::Y);
		Add(TEXT("Camera.RightAlt"), EAction::CameraFly, EKeys::Right, EContext::CameraKeys, EShape::Y);
		Add(TEXT("Camera.Left"), EAction::CameraFly, EKeys::A, EContext::CameraKeys, EShape::NegativeY);
		Add(TEXT("Camera.LeftAlt"), EAction::CameraFly, EKeys::Left, EContext::CameraKeys, EShape::NegativeY);
		Add(TEXT("Camera.Up"), EAction::CameraFly, EKeys::E, EContext::CameraKeys, EShape::Z);
		Add(TEXT("Camera.Down"), EAction::CameraFly, EKeys::Q, EContext::CameraKeys, EShape::NegativeZ);

		// --- Keys that mean one thing ---
		Add(TEXT("Key.Undo"), EAction::Undo, EKeys::BackSpace);
		Add(TEXT("Key.UndoAlt"), EAction::Undo, EKeys::Delete);
		Add(TEXT("Key.ClearShip"), EAction::ClearShip, EKeys::C);
		Add(TEXT("Key.Submit"), EAction::Submit, EKeys::Enter);
		Add(TEXT("Key.SkipPlayback"), EAction::SkipPlayback, EKeys::SpaceBar);
		Add(TEXT("Key.Back"), EAction::Back, EKeys::Escape);
		Add(TEXT("Key.ResetPuzzle"), EAction::ResetPuzzle, EKeys::F5);
		Add(TEXT("Key.NextShip"), EAction::NextShip, EKeys::RightBracket);
		Add(TEXT("Key.PreviousShip"), EAction::PreviousShip, EKeys::LeftBracket);
		Add(TEXT("Key.ShiftMoveLeft"), EAction::ShiftMoveLeft, EKeys::Comma);
		Add(TEXT("Key.ShiftMoveRight"), EAction::ShiftMoveRight, EKeys::Period);
		Add(TEXT("Key.ToggleKeyMode"), EAction::ToggleKeyMode, EKeys::Tab);
		Add(TEXT("Key.ToggleDetails"), EAction::ToggleDetails, EKeys::M);
		Add(TEXT("Key.ShowNames"), EAction::ShowNames, EKeys::LeftControl);
		Add(TEXT("Key.ShowNamesRight"), EAction::ShowNames, EKeys::RightControl);
		Add(TEXT("Key.ShowNamesAlt"), EAction::ShowNames, EKeys::LeftAlt);
		Add(TEXT("Key.ShowNamesAltRight"), EAction::ShowNames, EKeys::RightAlt);
		Add(TEXT("Key.CameraFast"), EAction::CameraFast, EKeys::LeftShift);
		Add(TEXT("Key.CameraFastRight"), EAction::CameraFast, EKeys::RightShift);
		Add(TEXT("Key.Reframe"), EAction::Reframe, EKeys::Home);
		Add(TEXT("Key.ToggleScriptMode"), EAction::ToggleScriptMode, EKeys::O);
		Add(TEXT("Key.EditBoard"), EAction::EditBoard, EKeys::B);
		Add(TEXT("Key.ToggleScriptedPreview"), EAction::ToggleScriptedPreview, EKeys::P);
		const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Digits); ++Index)
		{
			Add(*FString::Printf(TEXT("Key.SelectShip%d"), Index + 1), EAction::SelectShip, Digits[Index], EContext::Common, EShape::Plain, Index + 1.0f);
		}

		// --- Combinations ---
		Add(TEXT("Chord.SaveFile"), EAction::SaveFile, EKeys::S, EContext::Chords, EShape::Plain, 1.0f, EAction::ShowNames);
		Add(TEXT("Chord.ClearAll"), EAction::ClearAll, EKeys::C, EContext::Chords, EShape::Plain, 1.0f, EAction::CameraFast);

		// --- Gamepad (and so the Steam Deck) ---
		Add(TEXT("Pad.Forward"), EAction::MoveForward, EKeys::Gamepad_DPad_Up);
		Add(TEXT("Pad.YawLeft"), EAction::MoveYawLeft, EKeys::Gamepad_DPad_Left);
		Add(TEXT("Pad.YawRight"), EAction::MoveYawRight, EKeys::Gamepad_DPad_Right);
		Add(TEXT("Pad.Wait"), EAction::MoveWait, EKeys::Gamepad_DPad_Down);
		Add(TEXT("Pad.PitchUp"), EAction::MovePitchUp, EKeys::Gamepad_FaceButton_Top);
		Add(TEXT("Pad.PitchDown"), EAction::MovePitchDown, EKeys::Gamepad_FaceButton_Left);
		Add(TEXT("Pad.Submit"), EAction::Submit, EKeys::Gamepad_FaceButton_Bottom);
		Add(TEXT("Pad.Undo"), EAction::Undo, EKeys::Gamepad_FaceButton_Right);
		Add(TEXT("Pad.NextShip"), EAction::NextShip, EKeys::Gamepad_RightShoulder);
		Add(TEXT("Pad.PreviousShip"), EAction::PreviousShip, EKeys::Gamepad_LeftShoulder);
		Add(TEXT("Pad.Back"), EAction::Back, EKeys::Gamepad_Special_Right);
		// View / Select: names while held on the map, a restart in a level.
		Add(TEXT("Pad.ShowNames"), EAction::ShowNames, EKeys::Gamepad_Special_Left);
		Add(TEXT("Pad.ResetPuzzle"), EAction::ResetPuzzle, EKeys::Gamepad_Special_Left);
		Add(TEXT("Pad.ToggleDetails"), EAction::ToggleDetails, EKeys::Gamepad_LeftThumbstick);
		Add(TEXT("Pad.Reframe"), EAction::Reframe, EKeys::Gamepad_RightThumbstick);
		Add(TEXT("Pad.FlyForward"), EAction::CameraFly, EKeys::Gamepad_LeftY);
		Add(TEXT("Pad.FlyRight"), EAction::CameraFly, EKeys::Gamepad_LeftX, EContext::Common, EShape::Y);
		Add(TEXT("Pad.FlyUp"), EAction::CameraFly, EKeys::Gamepad_RightTriggerAxis, EContext::Common, EShape::Z);
		Add(TEXT("Pad.FlyDown"), EAction::CameraFly, EKeys::Gamepad_LeftTriggerAxis, EContext::Common, EShape::NegativeZ);
		Add(TEXT("Pad.TurnYaw"), EAction::CameraTurn, EKeys::Gamepad_RightX);
		Add(TEXT("Pad.TurnPitch"), EAction::CameraTurn, EKeys::Gamepad_RightY, EContext::Common, EShape::Y);
		return Bindings;
	}
}

const TArray<FGatherAndHuntInputBinding>& UGatherAndHuntInput::GetDefaultBindings()
{
	static const TArray<FGatherAndHuntInputBinding> Bindings = MakeDefaultBindings();
	return Bindings;
}

TArray<FGatherAndHuntInputBinding> UGatherAndHuntInput::GetBindings()
{
	TArray<FGatherAndHuntInputBinding> Bindings = GetDefaultBindings();
	for (const FGatherAndHuntKeyOverride& Override : UGatherAndHuntPlayerOptions::Get()->KeyOverrides)
	{
		for (FGatherAndHuntInputBinding& Binding : Bindings)
		{
			if (Binding.Id == Override.Id)
			{
				Binding.Key = Override.Key;
			}
		}
	}
	return Bindings;
}

FText UGatherAndHuntInput::GetActionName(const EGatherAndHuntInputAction Action)
{
	static const TCHAR* const Names[] = {
		TEXT("Forward"), TEXT("Turn left"), TEXT("Turn right"), TEXT("Nose up"), TEXT("Nose down"), TEXT("Wait"),
		TEXT("Undo"), TEXT("Clear ship"), TEXT("Clear all ships"), TEXT("Execute / launch"), TEXT("Skip playback"), TEXT("Back"), TEXT("Restart level"),
		TEXT("Next ship"), TEXT("Previous ship"), TEXT("Pick ship"), TEXT("Move earlier"), TEXT("Move later"),
		TEXT("Switch keys: camera / ship"), TEXT("Show / hide moves"), TEXT("Show level names"),
		TEXT("Fly camera"), TEXT("Turn camera"), TEXT("Fly faster"), TEXT("Reframe camera"),
		TEXT("Edit scripted ships"), TEXT("Edit board"), TEXT("Save board file"), TEXT("Preview scripted ships")};
	static_assert(UE_ARRAY_COUNT(Names) == ActionCount, "A name per action");
	return FText::FromString(Names[FMath::Clamp(static_cast<int32>(Action), 0, ActionCount - 1)]);
}

void UGatherAndHuntInput::Setup(APlayerController* PC)
{
	if (PC == nullptr || InputComponent != nullptr)
	{
		return;
	}
	PlayerController = PC;

	const UEnum* const ActionEnum = StaticEnum<EGatherAndHuntInputAction>();
	for (int32 Index = 0; Index < ActionCount; ++Index)
	{
		UInputAction* const Action = NewObject<UInputAction>(this, *FString::Printf(TEXT("IA_%s"), *ActionEnum->GetNameStringByIndex(Index)));
		Action->ValueType = ValueTypeOf(static_cast<EAction>(Index));
		// A key can sit in two contexts (W flies or moves); the contexts are
		// swapped rather than left to shadow each other.
		Action->bConsumeInput = false;
		Actions.Add(Action);
	}
	const UEnum* const ContextEnum = StaticEnum<EGatherAndHuntInputContext>();
	for (int32 Index = 0; Index < ContextCount; ++Index)
	{
		Contexts.Add(NewObject<UInputMappingContext>(this, *FString::Printf(TEXT("IMC_%s"), *ContextEnum->GetNameStringByIndex(Index))));
	}

	InputComponent = NewObject<UEnhancedInputComponent>(PC);
	InputComponent->RegisterComponent();
	for (int32 Index = 0; Index < ActionCount; ++Index)
	{
		const EAction Action = static_cast<EAction>(Index);
		// The axes and the held actions are polled; everything else is a press.
		if (Action != EAction::CameraFly && Action != EAction::CameraTurn && Action != EAction::CameraFast && Action != EAction::ShowNames)
		{
			InputComponent->BindAction(Actions[Index], ETriggerEvent::Started, this, &UGatherAndHuntInput::HandleStarted);
		}
	}
	PC->PushInputComponent(InputComponent);

	RebuildMappings();
}

void UGatherAndHuntInput::RebuildMappings()
{
	if (Contexts.Num() != ContextCount)
	{
		return;
	}
	for (UInputMappingContext* const Context : Contexts)
	{
		Context->UnmapAll();
	}

	for (const FGatherAndHuntInputBinding& Binding : GetBindings())
	{
		if (!Binding.Key.IsValid())
		{
			continue; // Unbound by the player.
		}
		UInputMappingContext* const Context = Contexts[static_cast<int32>(Binding.Context)];
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Actions[static_cast<int32>(Binding.Action)], Binding.Key);

		if (Binding.Key.IsAnalog())
		{
			UInputModifierDeadZone* const DeadZone = NewObject<UInputModifierDeadZone>(Context);
			DeadZone->LowerThreshold = StickDeadZone;
			Mapping.Modifiers.Add(DeadZone);
		}
		// A key's value arrives on X; move it to the axis it stands for.
		if (Binding.Shape == EShape::Y || Binding.Shape == EShape::NegativeY || Binding.Shape == EShape::Z || Binding.Shape == EShape::NegativeZ)
		{
			UInputModifierSwizzleAxis* const Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
			Swizzle->Order = Binding.Shape == EShape::Y || Binding.Shape == EShape::NegativeY ? EInputAxisSwizzle::YXZ : EInputAxisSwizzle::ZYX;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (Binding.Shape == EShape::Negative || Binding.Shape == EShape::NegativeY || Binding.Shape == EShape::NegativeZ)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
		if (Binding.Scale != 1.0f)
		{
			UInputModifierScalar* const Scalar = NewObject<UInputModifierScalar>(Context);
			Scalar->Scalar = FVector(Binding.Scale);
			Mapping.Modifiers.Add(Scalar);
		}
		if (Binding.Chord != EAction::Count)
		{
			UInputTriggerChordAction* const Chord = NewObject<UInputTriggerChordAction>(Context);
			Chord->ChordAction = Actions[static_cast<int32>(Binding.Chord)];
			Mapping.Triggers.Add(Chord);
		}
	}
	ApplyContexts();
}

void UGatherAndHuntInput::ApplyContexts()
{
	const ULocalPlayer* const LocalPlayer = PlayerController != nullptr ? PlayerController->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* const Subsystem = LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Subsystem == nullptr || Contexts.Num() != ContextCount)
	{
		return;
	}

	for (int32 Index = 0; Index < ContextCount; ++Index)
	{
		const EContext Context = static_cast<EContext>(Index);
		const bool bWanted = bEnabled
			&& (Context != EContext::CameraKeys || !bKeysMoveShip)
			&& (Context != EContext::ShipKeys || bKeysMoveShip);
		Subsystem->RemoveMappingContext(Contexts[Index]);
		if (bWanted)
		{
			Subsystem->AddMappingContext(Contexts[Index], PriorityOf(Context));
		}
	}
}

void UGatherAndHuntInput::SetEnabled(const bool bEnable)
{
	if (bEnabled != bEnable)
	{
		bEnabled = bEnable;
		ApplyContexts();
	}
}

void UGatherAndHuntInput::SetKeysMoveShip(const bool bMoveShip)
{
	if (bKeysMoveShip != bMoveShip)
	{
		bKeysMoveShip = bMoveShip;
		ApplyContexts();
	}
}

FVector UGatherAndHuntInput::GetAxis(const EGatherAndHuntInputAction Action) const
{
	const ULocalPlayer* const LocalPlayer = PlayerController != nullptr ? PlayerController->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* const Subsystem = LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const UEnhancedPlayerInput* const PlayerInput = Subsystem != nullptr ? Subsystem->GetPlayerInput() : nullptr;
	if (PlayerInput == nullptr || !bEnabled || !Actions.IsValidIndex(static_cast<int32>(Action)))
	{
		return FVector::ZeroVector;
	}
	return PlayerInput->GetActionValue(Actions[static_cast<int32>(Action)]).Get<FVector>();
}

bool UGatherAndHuntInput::IsHeld(const EGatherAndHuntInputAction Action) const
{
	return !GetAxis(Action).IsNearlyZero();
}

void UGatherAndHuntInput::HandleStarted(const FInputActionInstance& Instance)
{
	const int32 Index = Actions.IndexOfByKey(Instance.GetSourceAction());
	if (Index == INDEX_NONE || !bEnabled)
	{
		return;
	}
	const EAction Action = static_cast<EAction>(Index);

	// Ctrl+S is "save", not "save and wait": a plain key stays quiet while a
	// combination on the same key has its other half held.
	const TArray<FGatherAndHuntInputBinding> Bindings = GetBindings();
	for (const FGatherAndHuntInputBinding& Combination : Bindings)
	{
		if (Combination.Chord == EAction::Count || Combination.Action == Action || !IsHeld(Combination.Chord))
		{
			continue;
		}
		for (const FGatherAndHuntInputBinding& Binding : Bindings)
		{
			if (Binding.Action == Action && Binding.Key == Combination.Key)
			{
				return;
			}
		}
	}

	OnPressed.Broadcast(Action, static_cast<float>(Instance.GetValue().Get<FVector>().X));
}
