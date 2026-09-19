#pragma once

#include "CoreMinimal.h"
#include "IntVec3.h"
#include "Orientation.h"
#include "Move.h"

// Light conversion helpers between TurtleSim POD types and UE types.
// Adapters live in TurtleGame; TurtleSim itself remains free of UE deps.

namespace TurtleSimAdapt
{
	FORCEINLINE FIntVector ToFIntVector(const turtlesim::IntVec3& V)
	{
		return FIntVector(V.X, V.Y, V.Z);
	}

	FORCEINLINE turtlesim::IntVec3 FromFIntVector(const FIntVector& V)
	{
		return turtlesim::IntVec3(V.X, V.Y, V.Z);
	}

	FORCEINLINE FString HeadingToString(turtlesim::EHeading H)
	{
		switch (H)
		{
		case turtlesim::EHeading::North: return TEXT("North");
		case turtlesim::EHeading::East:  return TEXT("East");
		case turtlesim::EHeading::South: return TEXT("South");
		case turtlesim::EHeading::West:  return TEXT("West");
		}
		return TEXT("?");
	}

	FORCEINLINE FString PitchToString(turtlesim::EPitch P)
	{
		switch (P)
		{
		case turtlesim::EPitch::Up:    return TEXT("Up");
		case turtlesim::EPitch::Level: return TEXT("Level");
		case turtlesim::EPitch::Down:  return TEXT("Down");
		}
		return TEXT("?");
	}
}
