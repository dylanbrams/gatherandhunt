// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
UENUM(BlueprintType)
enum class PieceTypeEnum : uint8
{
	BP_BasePiece UMETA(DisplayName = "BP_BasePiece"),
	BP_PlayerOwnedPiece UMETA(DisplayName = "BP_PlayerOwnedPiece"),
	BP_Hazard UMETA(DisplayName = "BP_Hazard"),
	BP_CircularHazard UMETA(DisplayName = "BP_CircularHazard"),
	BP_LinearPatrolHazard UMETA(DisplayName = "BP_LinearPatrolHazard"),
	BP_Objective UMETA(DisplayName = "BP_Objective"),
	BP_GoldObjective UMETA(DisplayName = "BP_GoldObjective"),
};
