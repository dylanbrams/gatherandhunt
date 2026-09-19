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
