#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BoardState.h"
#include "Resolver.h"
#include "Match/ReplicatedMatchState.h"
#include "TurtleGameState.generated.h"

UCLASS()
class TURTLEGAME_API ATurtleGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnTurtleCachedStateChanged);

	ATurtleGameState();

	UPROPERTY(ReplicatedUsing = OnRep_MatchState, BlueprintReadOnly, Category = "Turtle|Match")
	FReplicatedMatchState MatchState;

	UFUNCTION(BlueprintCallable, Category = "Turtle|Match")
	EMatchPhase GetPhase() const { return MatchState.Phase; }

	UFUNCTION(BlueprintCallable, Category = "Turtle|Match")
	int32 GetTurnNumber() const { return MatchState.TurnNumber; }

	bool HasCachedBoardState() const { return bHasCachedBoardState; }
	const turtlesim::BoardState* GetCachedBoardState() const { return bHasCachedBoardState ? &CachedBoardState : nullptr; }
	bool HasLastReceivedTurnResult() const { return bHasLastReceivedTurnResult; }
	const turtlesim::TurnResult* GetLastReceivedTurnResult() const { return bHasLastReceivedTurnResult ? &LastReceivedTurnResult : nullptr; }
	const FOnTurtleCachedStateChanged& OnCachedStateChanged() const { return CachedStateChangedEvent; }

	// The board as it stood *before* the last received turn result, i.e. the state
	// playback should animate forward from. Null when it can't be recovered (a
	// mid-match join, or a full-state resync), in which case the caller should snap
	// to the final state instead of animating from it.
	const turtlesim::BoardState* GetPlaybackStartBoardState() const;

	void ApplyFullBoardState(const turtlesim::BoardState& InBoardState);
	void ApplyTurnResult(const turtlesim::TurnResult& InTurnResult);

	// Presenters that animate turn results register here. When nobody is registered
	// the PlayerController acknowledges playback as soon as the tick log arrives,
	// so a headless or spectating client doesn't stall the server for a full
	// PlaybackTimeoutSeconds every turn.
	void RegisterPlaybackObserver();
	void UnregisterPlaybackObserver();
	bool HasPlaybackObserver() const { return PlaybackObserverCount > 0; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_MatchState();

private:
	void RefreshCachedBoardStateFromReplication();

	// The replicated MatchState and the ClientReceiveTickLog RPC both carry the
	// post-turn board and can arrive in either order, so the previous board is
	// retained here rather than being derived at the call site: whichever of the
	// two lands first pushes the outgoing board aside, and the second one, having
	// the same TurnNumber, leaves it alone.
	void SetCachedBoardState(const turtlesim::BoardState& InBoardState);

	turtlesim::BoardState CachedBoardState;
	turtlesim::BoardState PreviousBoardState;
	turtlesim::TurnResult LastReceivedTurnResult;
	bool bHasCachedBoardState = false;
	bool bHasPreviousBoardState = false;
	bool bHasLastReceivedTurnResult = false;
	int32 PlaybackObserverCount = 0;
	FOnTurtleCachedStateChanged CachedStateChangedEvent;
};
