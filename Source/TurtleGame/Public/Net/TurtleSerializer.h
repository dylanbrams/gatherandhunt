#pragma once

#include "CoreMinimal.h"
#include "BoardState.h"
#include "Move.h"
#include "Resolver.h"
#include "Events.h"

// Wire serializers for TurtleSim types. Round-trip stable; not required to be
// byte-identical across platforms (the simulation itself is — the wire format is
// only required to deserialize back to an equal value).
//
// Format versioning: each Serialize* call emits a magic+version header.
// Bump the version when the layout changes; old payloads will fail to load.

namespace TurtleSimWire
{
	// BoardState <-> bytes.
	void SerializeBoardState(TArray<uint8>& OutBytes, const turtlesim::BoardState& State);
	bool DeserializeBoardState(const TArray<uint8>& InBytes, turtlesim::BoardState& OutState);

	// TurnPlan <-> bytes.
	void SerializeTurnPlan(TArray<uint8>& OutBytes, const turtlesim::TurnPlan& Plan);
	bool DeserializeTurnPlan(const TArray<uint8>& InBytes, turtlesim::TurnPlan& OutPlan);

	// TurnResult <-> bytes.
	void SerializeTurnResult(TArray<uint8>& OutBytes, const turtlesim::TurnResult& Result);
	bool DeserializeTurnResult(const TArray<uint8>& InBytes, turtlesim::TurnResult& OutResult);
}
