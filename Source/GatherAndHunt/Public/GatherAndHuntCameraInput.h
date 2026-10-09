#pragma once

#include "CoreMinimal.h"
#include "GatherAndHuntInput.h"
#include "GatherAndHuntPuzzleBoardView.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

// Free camera control shared by the screens that show a board (planner, level
// select map).
//
// Mouse, which the widget forwards: left drag orbits the point the camera is
// looking at, right drag looks around from where the camera is, middle drag
// slides it sideways and up/down.
//
// Keys and gamepad come through UGatherAndHuntInput's CameraFly, CameraTurn and
// CameraFast actions, applied each tick.
struct FGatherAndHuntCameraInput
{
	enum class EDrag : uint8 { None, Orbit, Look, Pan };

	void BeginDrag(const FPointerEvent& MouseEvent)
	{
		const FKey Button = MouseEvent.GetEffectingButton();
		Drag = Button == EKeys::RightMouseButton ? EDrag::Look
			: Button == EKeys::MiddleMouseButton ? EDrag::Pan
			: EDrag::Orbit;
	}

	bool IsDragging() const { return Drag != EDrag::None; }
	void EndDrag() { Drag = EDrag::None; }

	// True if the move was a camera drag.
	bool HandleMouseMove(AGatherAndHuntPuzzleBoardView* View, const FPointerEvent& MouseEvent) const
	{
		if (Drag == EDrag::None || View == nullptr)
		{
			return false;
		}
		const FVector2D Delta = MouseEvent.GetCursorDelta();
		switch (Drag)
		{
		case EDrag::Orbit: View->OrbitCamera(Delta.X * 0.3f, -Delta.Y * 0.3f); break;
		case EDrag::Look: View->LookCamera(Delta.X * 0.2f, -Delta.Y * 0.2f); break;
		case EDrag::Pan: View->PanCamera(-Delta.X, Delta.Y); break;
		default: break;
		}
		return true;
	}

	// Flies and turns the camera from the input actions' current values.
	static void Tick(AGatherAndHuntPuzzleBoardView* View, const UGatherAndHuntInput* Input, const float DeltaSeconds)
	{
		if (View == nullptr || Input == nullptr)
		{
			return;
		}
		const FVector Fly = Input->GetAxis(EGatherAndHuntInputAction::CameraFly);
		if (!Fly.IsNearlyZero())
		{
			View->FlyCamera(Fly, DeltaSeconds, Input->IsHeld(EGatherAndHuntInputAction::CameraFast));
		}
		const FVector Turn = Input->GetAxis(EGatherAndHuntInputAction::CameraTurn);
		if (!Turn.IsNearlyZero())
		{
			// Degrees per second at full stick.
			View->OrbitCamera(Turn.X * 120.0f * DeltaSeconds, Turn.Y * 90.0f * DeltaSeconds);
		}
	}

private:
	EDrag Drag = EDrag::None;
};
