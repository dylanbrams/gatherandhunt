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
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	PieceTypeEnum PieceType = PieceTypeEnum::BP_BasePiece; // Or use an enum, if you have a finite set of piece types

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int PieceSaveID = 0; // Or use an enum, if you have a finite set of piece types

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int PieceOwnerID = 0; // Or use an enum, if you have a finite set of piece types
	
};
