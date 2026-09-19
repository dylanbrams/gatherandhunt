#pragma once

#include "CoreMinimal.h"
#include "Match/MatchPhase.h"
#include "ReplicatedMatchState.generated.h"

// Replicated to all clients on the GameState. Updates only at phase transitions.
// SerializedBoardState is the wire form of turtlesim::BoardState (see TurtleSerializer).
USTRUCT(BlueprintType)
struct TURTLEGAME_API FReplicatedMatchState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint8> SerializedBoardState;

	UPROPERTY(BlueprintReadOnly)
	int32 TurnNumber = 0;

	UPROPERTY(BlueprintReadOnly)
	EMatchPhase Phase = EMatchPhase::WaitingForPlayers;

	UPROPERTY(BlueprintReadOnly)
	int32 ScorePlayer0 = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 ScorePlayer1 = 0;

	// Bookkeeping per planning phase: which players have submitted.
	UPROPERTY(BlueprintReadOnly)
	bool bPlayer0Submitted = false;

	UPROPERTY(BlueprintReadOnly)
	bool bPlayer1Submitted = false;
};
