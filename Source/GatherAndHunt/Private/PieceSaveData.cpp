// Fill out your copyright notice in the Description page of Project Settings.


#include "PieceSaveData.h"

namespace
{
	int32 PositiveStepCount(int32 Value)
	{
		return FMath::Max(1, Value);
	}

	int32 WrapIndex(int32 Index, int32 Count)
	{
		if (Count <= 0)
		{
			return 0;
		}
		const int32 Mod = Index % Count;
		return Mod >= 0 ? Mod : Mod + Count;
	}

	void AddUniquePoint(TArray<FVector>& Points, const FVector& Point)
	{
		if (Points.Num() == 0 || !Points.Last().Equals(Point))
		{
			Points.Add(Point);
		}
	}

	FVector SnapToAxis(const FVector& Vector)
	{
		const FVector Normalized = Vector.GetSafeNormal();
		if (Normalized.IsNearlyZero())
		{
			return FVector::ForwardVector;
		}

		const float AbsX = FMath::Abs(Normalized.X);
		const float AbsY = FMath::Abs(Normalized.Y);
		const float AbsZ = FMath::Abs(Normalized.Z);

		if (AbsX >= AbsY && AbsX >= AbsZ)
		{
			return FVector(FMath::Sign(Normalized.X), 0.0f, 0.0f);
		}
		if (AbsY >= AbsX && AbsY >= AbsZ)
		{
			return FVector(0.0f, FMath::Sign(Normalized.Y), 0.0f);
		}
		return FVector(0.0f, 0.0f, FMath::Sign(Normalized.Z));
	}
}

bool FPieceSaveData::IsCircularHazard() const
{
	return PieceType == PieceTypeEnum::BP_CircularHazard
		|| MovementType == MovementTypeEnum::BP_AbsoluteMoveCircle;
}

bool FPieceSaveData::IsLinearPatrolHazard() const
{
	return PieceType == PieceTypeEnum::BP_LinearPatrolHazard
		|| MovementType == MovementTypeEnum::BP_AbsoluteMoveLinearPatrol;
}

bool FPieceSaveData::IsDeterministicHazard() const
{
	return IsCircularHazard() || IsLinearPatrolHazard();
}

void FPieceSaveData::GetDeterministicHazardPath(TArray<FVector>& OutPath) const
{
	OutPath = IsCircularHazard()
		? BuildCircularHazardPath()
		: BuildLinearPatrolPath();

	if (OutPath.Num() == 0)
	{
		OutPath.Add(Position);
	}
}

int32 FPieceSaveData::GetDeterministicHazardPathLength() const
{
	TArray<FVector> Path;
	GetDeterministicHazardPath(Path);
	return Path.Num();
}

FVector FPieceSaveData::GetDeterministicHazardPositionAtTurn(int32 TurnIndex) const
{
	if (TurnIndex <= 0 || !IsDeterministicHazard())
	{
		return Position;
	}

	TArray<FVector> Path;
	GetDeterministicHazardPath(Path);

	if (Path.Num() == 0)
	{
		return Position;
	}

	const int32 PathIndex = WrapIndex(TurnIndex + HazardPhaseOffset, Path.Num());
	return Path[PathIndex];
}

TArray<FVector> FPieceSaveData::BuildCircularHazardPath() const
{
	TArray<FVector> Path;

	const int32 ForwardSteps = PositiveStepCount(HazardPrimaryStepCount);
	const int32 RightSteps = PositiveStepCount(HazardSecondaryStepCount);
	const FVector Forward = SnapToAxis(Rotation.RotateVector(FVector::ForwardVector)) * HazardStepDistance;
	const FVector Right = SnapToAxis(Rotation.RotateVector(FVector::RightVector)) * HazardStepDistance;

	for (int32 Step = 0; Step < RightSteps; ++Step)
	{
		AddUniquePoint(Path, Position + (Right * Step));
	}
	for (int32 Step = 0; Step < ForwardSteps; ++Step)
	{
		AddUniquePoint(Path, Position + (Right * RightSteps) + (Forward * Step));
	}
	for (int32 Step = 0; Step < RightSteps; ++Step)
	{
		AddUniquePoint(Path, Position + (Right * (RightSteps - Step)) + (Forward * ForwardSteps));
	}
	for (int32 Step = 0; Step < ForwardSteps; ++Step)
	{
		AddUniquePoint(Path, Position + (Forward * (ForwardSteps - Step)));
	}

	if (Path.Num() == 0)
	{
		Path.Add(Position);
	}

	return Path;
}

TArray<FVector> FPieceSaveData::BuildLinearPatrolPath() const
{
	TArray<FVector> Path;

	const int32 ForwardSteps = PositiveStepCount(HazardPrimaryStepCount);
	const FVector Forward = SnapToAxis(Rotation.RotateVector(FVector::ForwardVector)) * HazardStepDistance;

	for (int32 Step = 0; Step <= ForwardSteps; ++Step)
	{
		AddUniquePoint(Path, Position + (Forward * Step));
	}
	for (int32 Step = ForwardSteps - 1; Step >= 1; --Step)
	{
		AddUniquePoint(Path, Position + (Forward * Step));
	}

	if (Path.Num() == 0)
	{
		Path.Add(Position);
	}

	return Path;
}
