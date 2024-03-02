// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PieceTypeEnum.h"
#include "PieceSaveData.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FPieceSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Scale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	PieceTypeEnum PieceType; // Or use an enum, if you have a finite set of piece types

	// Add other necessary properties here
};
