#pragma once

#include "CoreMinimal.h"
#include "MatchPhase.generated.h"

UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	WaitingForPlayers UMETA(DisplayName = "Waiting For Players"),
	Planning          UMETA(DisplayName = "Planning"),
	AwaitingPlans     UMETA(DisplayName = "Awaiting Plans"),
	Resolving         UMETA(DisplayName = "Resolving"),
	Animating         UMETA(DisplayName = "Animating"),
	Ended             UMETA(DisplayName = "Ended"),
};

// Why the match ended. Set alongside the winner when the Ended phase is entered.
UENUM(BlueprintType)
enum class EMatchEndReason : uint8
{
	NotEnded       UMETA(DisplayName = "Not Ended"),
	TurnLimit      UMETA(DisplayName = "Turn Limit Reached"),
	// A side has no turtles left on the board, whether destroyed or delivered.
	TeamOutOfPlay  UMETA(DisplayName = "Team Out Of Play"),
	ObjectivesGone UMETA(DisplayName = "All Objectives Resolved"),
};
