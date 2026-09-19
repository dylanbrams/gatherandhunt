#include "Playback/BoardPresenter.h"

#include "Playback/TurtleActor.h"
#include "Playback/ObstacleActor.h"
#include "Playback/ObjectiveActor.h"
#include "Playback/TurtlePlaybackController.h"
#include "Match/TurtleGameState.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

#include "BoardState.h"
#include "Resolver.h"

ABoardPresenter::ABoardPresenter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Playback = CreateDefaultSubobject<UTurtlePlaybackController>(TEXT("Playback"));

	TurtleActorClass    = ATurtleActor::StaticClass();
	ObstacleActorClass  = AObstacleActor::StaticClass();
	ObjectiveActorClass = AObjectiveActor::StaticClass();
}

void ABoardPresenter::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (ATurtleGameState* GS = World->GetGameState<ATurtleGameState>())
		{
			ObservedGameState = GS;
			// OnCachedStateChanged() returns a const& by design; cast to subscribe.
			// (Alternative: add a non-const accessor on ATurtleGameState.)
			auto& Delegate = const_cast<ATurtleGameState::FOnTurtleCachedStateChanged&>(GS->OnCachedStateChanged());
			CachedStateChangedHandle = Delegate.AddUObject(this, &ABoardPresenter::HandleCachedStateChanged);

			// If state was already replicated before BeginPlay, sync now.
			if (GS->HasCachedBoardState())
			{
				HandleCachedStateChanged();
			}
		}
	}
}

void ABoardPresenter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ATurtleGameState* GS = ObservedGameState.Get())
	{
		auto& Delegate = const_cast<ATurtleGameState::FOnTurtleCachedStateChanged&>(GS->OnCachedStateChanged());
		Delegate.Remove(CachedStateChangedHandle);
	}
	DespawnAll();
	Super::EndPlay(Reason);
}

void ABoardPresenter::HandleCachedStateChanged()
{
	ATurtleGameState* GS = ObservedGameState.Get();
	if (!GS) return;

	const turtlesim::BoardState* CachedState = GS->GetCachedBoardState();
	if (!CachedState) return;

	// Ensure visuals exist for every entity. Spawning is idempotent: if an actor
	// already exists for an Id, we reuse it.
	SyncSpawnsToBoardState(*CachedState);

	const turtlesim::TurnResult* Result = GS->GetLastReceivedTurnResult();
	if (Result && Playback && Result->FinalState.TurnNumber != LastAnimatedTurn)
	{
		LastAnimatedTurn = Result->FinalState.TurnNumber;
		// We don't have the start-of-turn state directly; use the *previous* cached
		// state if we tracked it. For v1 simplicity, snap to the start by replaying
		// from the FinalState's negation isn't possible — instead we snap to the
		// current visuals and let the controller's per-tick targets walk forward.
		// The controller's ApplyBoardStateInstant inside StartPlayback uses the
		// passed-in StartState, so we pass a synthesized "state minus this turn's
		// movement" by taking the ResetTo positions from event FromPositions where
		// available. Pragmatic: just animate forward from current visuals.
		//
		// We pass FinalState here as the "instant snap target" only if there are no
		// ticks; with ticks present, BeginTick will set targets explicitly per event.
		Playback->StartPlayback(*CachedState, *Result);
	}
	else if (Playback && !Playback->IsPlaying())
	{
		Playback->ApplyBoardStateInstant(*CachedState);
	}
}

void ABoardPresenter::SyncSpawnsToBoardState(const turtlesim::BoardState& State)
{
	UWorld* World = GetWorld();
	if (!World || !Playback) return;

	for (const auto& T : State.Turtles)
	{
		if (!SpawnedTurtles.Contains(T.Id))
		{
			TSubclassOf<ATurtleActor> Cls = TurtleActorClass ? TurtleActorClass : TSubclassOf<ATurtleActor>(ATurtleActor::StaticClass());
			ATurtleActor* A = World->SpawnActor<ATurtleActor>(Cls, FVector::ZeroVector, FRotator::ZeroRotator);
			if (!A) continue;
			A->TurtleId = T.Id;
			A->OwnerSlotIndex = T.Owner;
			SpawnedTurtles.Add(T.Id, A);
			Playback->RegisterTurtle(T.Id, A);
		}
	}

	for (const auto& O : State.Obstacles)
	{
		if (!SpawnedObstacles.Contains(O.Id))
		{
			TSubclassOf<AObstacleActor> Cls = ObstacleActorClass ? ObstacleActorClass : TSubclassOf<AObstacleActor>(AObstacleActor::StaticClass());
			AObstacleActor* A = World->SpawnActor<AObstacleActor>(Cls, FVector::ZeroVector, FRotator::ZeroRotator);
			if (!A) continue;
			A->ObstacleId = O.Id;
			A->Kind = static_cast<int32>(O.Kind);
			SpawnedObstacles.Add(O.Id, A);
			Playback->RegisterObstacle(O.Id, A);
		}
	}

	for (const auto& J : State.Objectives)
	{
		if (!SpawnedObjectives.Contains(J.Id))
		{
			TSubclassOf<AObjectiveActor> Cls = ObjectiveActorClass ? ObjectiveActorClass : TSubclassOf<AObjectiveActor>(AObjectiveActor::StaticClass());
			AObjectiveActor* A = World->SpawnActor<AObjectiveActor>(Cls, FVector::ZeroVector, FRotator::ZeroRotator);
			if (!A) continue;
			A->ObjectiveId = J.Id;
			A->PointValue = J.PointValue;
			SpawnedObjectives.Add(J.Id, A);
			Playback->RegisterObjective(J.Id, A);
		}
	}
}

void ABoardPresenter::DespawnAll()
{
	for (auto& Pair : SpawnedTurtles)    if (Pair.Value)    Pair.Value->Destroy();
	for (auto& Pair : SpawnedObstacles)  if (Pair.Value)    Pair.Value->Destroy();
	for (auto& Pair : SpawnedObjectives) if (Pair.Value)    Pair.Value->Destroy();
	SpawnedTurtles.Reset();
	SpawnedObstacles.Reset();
	SpawnedObjectives.Reset();
	if (Playback) Playback->ClearRegistered();
}
