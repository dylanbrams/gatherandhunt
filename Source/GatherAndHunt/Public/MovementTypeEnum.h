#pragma once
#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class MovementTypeEnum :uint8 {
	None UMETA(DisplayName = "None"),
	BP_ForwardTwoDown UMETA(DisplayName = "BP_ForwardTwoDown"),
	BP_ForwardTwoTurnClockwise UMETA(DisplayName = "BP_ForwardTwoTurnClockwise"),
	BP_ChaseNearest UMETA(DisplayName = "BP_ChaseNearest"),
	BP_ChaseTarget UMETA(DisplayName = "BP_ChaseTarget"),
	BP_AbsoluteMoveCircle UMETA(DisplayName = "BP_AbsoluteMoveCircle"),
	BP_AbsoluteMoveLinearPatrol UMETA(DisplayName = "BP_AbsoluteMoveLinearPatrol")
};

