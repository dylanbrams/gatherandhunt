// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PieceTypeEnum.h"
#include "MovementTypeEnum.h"
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
	PieceTypeEnum PieceType = PieceTypeEnum::BP_BasePiece;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	MovementTypeEnum MovementType = MovementTypeEnum::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool CanHaveQueue = false;

	// Turtles only. With PieceOwnerID 0, true makes the turtle an ally moved by
	// the puzzle's scripted turns instead of by the player. Opponent turtles
	// (PieceOwnerID 1) are always scripted. See UGatherAndHuntLevelEditorLibrary::GetTurtleRole.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bScriptControlled = false;

	// Deterministic hazard motion authoring. Position is the origin/start point.
	// Rotation defines the forward/right axes used by patrol and loop motion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HazardStepDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 HazardPrimaryStepCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 HazardSecondaryStepCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 HazardPhaseOffset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int PieceSaveID = 0; // Or use an enum, if you have a finite set of piece types

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int PieceOwnerID = 0; // Or use an enum, if you have a finite set of piece types

	bool IsCircularHazard() const;
	bool IsLinearPatrolHazard() const;
	bool IsDeterministicHazard() const;
	void GetDeterministicHazardPath(TArray<FVector>& OutPath) const;
	int32 GetDeterministicHazardPathLength() const;
	FVector GetDeterministicHazardPositionAtTurn(int32 TurnIndex) const;

private:
	TArray<FVector> BuildCircularHazardPath() const;
	TArray<FVector> BuildLinearPatrolPath() const;

	
	
};
