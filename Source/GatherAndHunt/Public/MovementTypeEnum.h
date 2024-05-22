#pragma once
#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class MovementTypeEnum :uint8 {
	BP_ForwardTwoDown UMETA(DisplayName = "BP_ForwardOneDown"),
	BP_ForwardTwoTurnClockwise UMETA(DisplayName = "BP_ForwardTwoTurnClockwise"),
	BP_ChaseNearest UMETA(DisplayName = "BP_ChaseNearest"),
	BP_ChaseTarget UMETA(DisplayName = "BP_ChaseTarget"),
	BP_AbsoluteMoveCircle UMETA(DisplayName = "BP_AbsoluteMoveCircle")
};

