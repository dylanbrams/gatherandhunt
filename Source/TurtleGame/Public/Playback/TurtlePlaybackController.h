#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BoardState.h"
#include "Resolver.h"
#include "TurtlePlaybackController.generated.h"

class ATurtleActor;
class AObstacleActor;
class AObjectiveActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTurtlePlaybackSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTurtlePlaybackTickEvent, int32, TickIndex);

// Drives playback of a turtlesim::TurnResult against registered visual actors.
// Mirrors UGatherAndHuntPuzzlePlaybackComponent on the SP side, but consumes
// the simulation types directly (GameState already deserialized them) and
// fires acknowledgements to the local PlayerController on completion.
UCLASS(ClassGroup=(TurtleGame), meta=(BlueprintSpawnableComponent))
class TURTLEGAME_API UTurtlePlaybackController : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurtlePlaybackController();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turtle|Playback")
	float SecondsPerTick = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turtle|Playback")
	float GridCellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turtle|Playback")
	bool bAutoAcknowledgeServer = true;

	UPROPERTY(BlueprintAssignable) FTurtlePlaybackSimpleEvent OnPlaybackStarted;
	UPROPERTY(BlueprintAssignable) FTurtlePlaybackTickEvent   OnPlaybackTickStarted;
	UPROPERTY(BlueprintAssignable) FTurtlePlaybackTickEvent   OnPlaybackTickFinished;
	UPROPERTY(BlueprintAssignable) FTurtlePlaybackSimpleEvent OnPlaybackFinished;

	UFUNCTION(BlueprintCallable, Category = "Turtle|Playback")
	void RegisterTurtle(int32 TurtleId, ATurtleActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Turtle|Playback")
	void RegisterObstacle(int32 ObstacleId, AObstacleActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Turtle|Playback")
	void RegisterObjective(int32 ObjectiveId, AObjectiveActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Turtle|Playback")
	void ClearRegistered();

	// Snaps every registered actor to its position from a freshly-replicated BoardState.
	void ApplyBoardStateInstant(const turtlesim::BoardState& State);

	// Walks the tick log; advances at SecondsPerTick. Calls ServerAcknowledgePlayback
	// on the owning local PlayerController on completion (if bAutoAcknowledgeServer).
	void StartPlayback(const turtlesim::BoardState& StartState, const turtlesim::TurnResult& Result);

	UFUNCTION(BlueprintCallable, Category = "Turtle|Playback")
	void StopPlayback();

	UFUNCTION(BlueprintPure, Category = "Turtle|Playback")
	bool IsPlaying() const { return bIsPlaying; }

	UFUNCTION(BlueprintPure, Category = "Turtle|Playback")
	int32 GetCurrentTickIndex() const { return ActiveTickIndex; }

private:
	void BeginTick(int32 TickIndex);
	void FinishCurrentTick();
	void FinishPlayback();

	FVector  GridToWorld(const turtlesim::IntVec3& Grid) const;
	FRotator OrientationToWorldRotation(const turtlesim::Orientation& O) const;

	UPROPERTY() TMap<int32, TWeakObjectPtr<ATurtleActor>>    Turtles;
	UPROPERTY() TMap<int32, TWeakObjectPtr<AObstacleActor>>  Obstacles;
	UPROPERTY() TMap<int32, TWeakObjectPtr<AObjectiveActor>> Objectives;

	turtlesim::BoardState ActiveStartState;
	turtlesim::TurnResult ActiveResult;

	bool  bIsPlaying = false;
	int32 ActiveTickIndex = INDEX_NONE;
	float TickElapsed = 0.0f;
	int32 PendingAckTurnNumber = INDEX_NONE;
};
