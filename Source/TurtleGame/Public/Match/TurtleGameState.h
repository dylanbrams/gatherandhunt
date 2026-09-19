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

	void ApplyFullBoardState(const turtlesim::BoardState& InBoardState);
	void ApplyTurnResult(const turtlesim::TurnResult& InTurnResult);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_MatchState();

private:
	void RefreshCachedBoardStateFromReplication();

	turtlesim::BoardState CachedBoardState;
	turtlesim::TurnResult LastReceivedTurnResult;
	bool bHasCachedBoardState = false;
	bool bHasLastReceivedTurnResult = false;
	FOnTurtleCachedStateChanged CachedStateChangedEvent;
};
