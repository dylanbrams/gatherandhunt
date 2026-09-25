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

void ATurtleGameState::SetCachedBoardState(const turtlesim::BoardState& InBoardState)
{
	if (bHasCachedBoardState && CachedBoardState.TurnNumber != InBoardState.TurnNumber)
	{
		PreviousBoardState = CachedBoardState;
		bHasPreviousBoardState = true;
	}
	CachedBoardState = InBoardState;
	bHasCachedBoardState = true;
}

const turtlesim::BoardState* ATurtleGameState::GetPlaybackStartBoardState() const
{
	if (bHasPreviousBoardState
		&& bHasLastReceivedTurnResult
		&& PreviousBoardState.TurnNumber == LastReceivedTurnResult.FinalState.TurnNumber - 1)
	{
		return &PreviousBoardState;
	}
	return nullptr;
}

void ATurtleGameState::ApplyFullBoardState(const turtlesim::BoardState& InBoardState)
{
	// An authoritative resync: nothing to animate, and any retained previous board
	// is not necessarily this board's predecessor.
	SetCachedBoardState(InBoardState);
	PreviousBoardState = turtlesim::BoardState{};
	bHasPreviousBoardState = false;
	LastReceivedTurnResult = turtlesim::TurnResult{};
	bHasLastReceivedTurnResult = false;
	CachedStateChangedEvent.Broadcast();
}

void ATurtleGameState::ApplyTurnResult(const turtlesim::TurnResult& InTurnResult)
{
	LastReceivedTurnResult = InTurnResult;
	bHasLastReceivedTurnResult = true;
	SetCachedBoardState(InTurnResult.FinalState);
	CachedStateChangedEvent.Broadcast();
}

void ATurtleGameState::RegisterPlaybackObserver()
{
	++PlaybackObserverCount;
}

void ATurtleGameState::UnregisterPlaybackObserver()
{
	PlaybackObserverCount = FMath::Max(0, PlaybackObserverCount - 1);
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
		SetCachedBoardState(Decoded);
		CachedStateChangedEvent.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TurtleGameState: failed to deserialize replicated board state"));
	}
}
