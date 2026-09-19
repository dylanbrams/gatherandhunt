#include "Match/TurtleGameState.h"

#include "Net/UnrealNetwork.h"
#include "Net/TurtleSerializer.h"

ATurtleGameState::ATurtleGameState()
{
	bReplicates = true;
}

void ATurtleGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATurtleGameState, MatchState);
}

void ATurtleGameState::OnRep_MatchState()
{
	// Hook for Blueprint or HUD: phase changed, scores updated, etc.
	// Clients can detect transitions here and trigger UI updates / playback.
	RefreshCachedBoardStateFromReplication();
}

void ATurtleGameState::ApplyFullBoardState(const turtlesim::BoardState& InBoardState)
{
	CachedBoardState = InBoardState;
	bHasCachedBoardState = true;
	LastReceivedTurnResult = turtlesim::TurnResult{};
	bHasLastReceivedTurnResult = false;
	CachedStateChangedEvent.Broadcast();
}

void ATurtleGameState::ApplyTurnResult(const turtlesim::TurnResult& InTurnResult)
{
	LastReceivedTurnResult = InTurnResult;
	bHasLastReceivedTurnResult = true;
	CachedBoardState = InTurnResult.FinalState;
	bHasCachedBoardState = true;
	CachedStateChangedEvent.Broadcast();
}

void ATurtleGameState::RefreshCachedBoardStateFromReplication()
{
	if (MatchState.SerializedBoardState.Num() == 0)
	{
		return;
	}

	turtlesim::BoardState Decoded;
	if (TurtleSimWire::DeserializeBoardState(MatchState.SerializedBoardState, Decoded))
	{
		CachedBoardState = std::move(Decoded);
		bHasCachedBoardState = true;
		CachedStateChangedEvent.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TurtleGameState: failed to deserialize replicated board state"));
	}
}
