#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "GatherAndHuntInput.generated.h"

class APlayerController;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionInstance;

// Everything a player can do on the board screens (planner, level select map).
// Each is an Enhanced Input action; the screens act on these, never on keys.
UENUM()
enum class EGatherAndHuntInputAction : uint8
{
	// Queue a move for the selected ship.
	MoveForward,
	MoveYawLeft,
	MoveYawRight,
	MovePitchUp,
	MovePitchDown,
	MoveWait,

	Undo,
	ClearShip,
	ClearAll,
	// Execute / launch, or skip the playback.
	Submit,
	SkipPlayback,
	// Deselect, or leave the screen.
	Back,
	ResetPuzzle,

	NextShip,
	PreviousShip,
	// Value 1-9: the ship in that row.
	SelectShip,
	ShiftMoveLeft,
	ShiftMoveRight,

	// Switches the letter keys between the camera and the ship.
	ToggleKeyMode,
	ToggleDetails,
	// Held: level names on the map.
	ShowNames,

	// Axis (forward, right, up), each -1..1.
	CameraFly,
	// Axis (yaw, pitch), each -1..1.
	CameraTurn,
	// Held.
	CameraFast,
	Reframe,

	// Authoring.
	ToggleScriptMode,
	EditBoard,
	SaveFile,
	ToggleScriptedPreview,

	Count UMETA(Hidden)
};

// Which mapping context a binding belongs to.
UENUM()
enum class EGatherAndHuntInputContext : uint8
{
	// Always on: the gamepad, and the keys that mean one thing.
	Common,
	// The letter keys while they fly the camera (the default).
	CameraKeys,
	// The letter keys while they move the ship (after Tab).
	ShipKeys,
	// Key combinations, which outrank the plain key.
	Chords,

	Count UMETA(Hidden)
};

// How a key's value lands on its action's axes.
UENUM()
enum class EGatherAndHuntInputShape : uint8
{
	Plain,      // A press, or +X.
	Negative,   // -X.
	Y,
	NegativeY,
	Z,
	NegativeZ
};

// One row of the bindings table: a key (or stick, or trigger) for an action.
USTRUCT()
struct GATHERANDHUNT_API FGatherAndHuntInputBinding
{
	GENERATED_BODY()

	// Stable name for this row; a player's override refers to it.
	UPROPERTY()
	FName Id;

	UPROPERTY()
	EGatherAndHuntInputAction Action = EGatherAndHuntInputAction::Count;

	UPROPERTY()
	FKey Key;

	UPROPERTY()
	EGatherAndHuntInputContext Context = EGatherAndHuntInputContext::Common;

	UPROPERTY()
	EGatherAndHuntInputShape Shape = EGatherAndHuntInputShape::Plain;

	// Multiplies the value (SelectShip's row number).
	UPROPERTY()
	float Scale = 1.0f;

	// Not Count: only fires while this other action is held.
	UPROPERTY()
	EGatherAndHuntInputAction Chord = EGatherAndHuntInputAction::Count;
};

// A player's change to one row of the bindings table.
USTRUCT()
struct GATHERANDHUNT_API FGatherAndHuntKeyOverride
{
	GENERATED_BODY()

	// FGatherAndHuntInputBinding::Id.
	UPROPERTY(Config)
	FName Id;

	// The key to use instead; an invalid key unbinds the row.
	UPROPERTY(Config)
	FKey Key;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FGatherAndHuntInputPressed, EGatherAndHuntInputAction, float);

// The board screens' input, built on Enhanced Input without any assets: an
// Input Action per EGatherAndHuntInputAction and a mapping context per
// EGatherAndHuntInputContext, made in code from the bindings table. Keyboard
// and gamepad (so the Steam Deck) are both in the table.
//
// The screens never read keys. They listen to OnPressed and poll IsHeld /
// GetAxis, so rebinding is only a change to the table.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntInput : public UObject
{
	GENERATED_BODY()

public:
	// The built-in bindings.
	static const TArray<FGatherAndHuntInputBinding>& GetDefaultBindings();

	// The built-in bindings with the player's overrides
	// (UGatherAndHuntPlayerOptions::KeyOverrides) applied.
	static TArray<FGatherAndHuntInputBinding> GetBindings();

	// Player-facing name of an action, for a controls screen.
	static FText GetActionName(EGatherAndHuntInputAction Action);

	// Makes the actions and contexts and starts listening on PC. The keyboard
	// and gamepad then reach this object whenever the game viewport has focus.
	void Setup(APlayerController* PC);

	// Rebuilds the contexts from GetBindings, after the overrides change.
	void RebuildMappings();

	// Off while another screen (the board editor) owns the keyboard.
	void SetEnabled(bool bEnable);

	// Whether the letter keys move the ship rather than fly the camera.
	void SetKeysMoveShip(bool bMoveShip);
	bool DoKeysMoveShip() const { return bKeysMoveShip; }

	bool IsHeld(EGatherAndHuntInputAction Action) const;
	FVector GetAxis(EGatherAndHuntInputAction Action) const;

	// A press of a non-axis action; the value is 1 except for SelectShip.
	FGatherAndHuntInputPressed OnPressed;

private:
	void HandleStarted(const FInputActionInstance& Instance);
	void ApplyContexts();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> Actions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputMappingContext>> Contexts;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> InputComponent;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController;

	bool bEnabled = true;
	bool bKeysMoveShip = false;
};
