#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoardPresenter.generated.h"

class ATurtleActor;
class AObstacleActor;
class AObjectiveActor;
class UTurtlePlaybackController;
class ATurtleGameState;

namespace turtlesim { struct BoardState; struct TurnResult; }

// Owns the playback controller and spawns/destroys visual actors as the cached
// board state on the GameState changes. Drop one into a gameplay map.
UCLASS(Blueprintable)
class TURTLEGAME_API ABoardPresenter : public AActor
{
	GENERATED_BODY()

public:
	ABoardPresenter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turtle|Presenter")
	TSubclassOf<ATurtleActor> TurtleActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turtle|Presenter")
	TSubclassOf<AObstacleActor> ObstacleActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turtle|Presenter")
	TSubclassOf<AObjectiveActor> ObjectiveActorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turtle|Presenter")
	TObjectPtr<UTurtlePlaybackController> Playback;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void HandleCachedStateChanged();
	void SyncSpawnsToBoardState(const turtlesim::BoardState& State);
	void DespawnAll();

	UPROPERTY() TMap<int32, TObjectPtr<ATurtleActor>>    SpawnedTurtles;
	UPROPERTY() TMap<int32, TObjectPtr<AObstacleActor>>  SpawnedObstacles;
	UPROPERTY() TMap<int32, TObjectPtr<AObjectiveActor>> SpawnedObjectives;

	FDelegateHandle CachedStateChangedHandle;
	TWeakObjectPtr<ATurtleGameState> ObservedGameState;
	int32 LastAnimatedTurn = INDEX_NONE;
	bool bRegisteredPlaybackObserver = false;
};
