#include "GatherAndHuntPuzzlePlaybackComponent.h"

#include "GameFramework/Actor.h"
#include "GatherAndHuntPuzzleSession.h"

namespace
{
	bool EventAffectsRotation(const EGatherAndHuntPuzzleEventType EventType)
	{
		return EventType == EGatherAndHuntPuzzleEventType::TurtleTurned
			|| EventType == EGatherAndHuntPuzzleEventType::TurtleMoved
			|| EventType == EGatherAndHuntPuzzleEventType::TurtleBlocked;
	}
}

UGatherAndHuntPuzzlePlaybackComponent::UGatherAndHuntPuzzlePlaybackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UGatherAndHuntPuzzlePlaybackComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsPlaying || bIsPaused || ActiveTickIndex == INDEX_NONE)
	{
		return;
	}

	const float EffectiveSecondsPerTick = FMath::Max(0.001f, SecondsPerTick);
	TickElapsedSeconds += DeltaTime;
	const float Alpha = FMath::Clamp(TickElapsedSeconds / EffectiveSecondsPerTick, 0.0f, 1.0f);
	UpdateActorsForInterpolation(Alpha);

	if (TickElapsedSeconds >= EffectiveSecondsPerTick)
	{
		FinishTickPlayback();
	}
}

void UGatherAndHuntPuzzlePlaybackComponent::RegisterActor(
	const EGatherAndHuntPuzzlePlaybackEntityKind EntityKind,
	const int32 EntityId,
	AActor* Actor)
{
	if (Actor == nullptr)
	{
		return;
	}

	FPlaybackBinding Binding;
	Binding.Actor = Actor;
	Binding.StartLocation = Actor->GetActorLocation();
	Binding.TargetLocation = Binding.StartLocation;
	Binding.StartRotation = Actor->GetActorRotation();
	Binding.TargetRotation = Binding.StartRotation;

	switch (EntityKind)
	{
	case EGatherAndHuntPuzzlePlaybackEntityKind::Turtle:
		TurtleBindings.Add(EntityId, Binding);
		break;
	case EGatherAndHuntPuzzlePlaybackEntityKind::Obstacle:
		ObstacleBindings.Add(EntityId, Binding);
		break;
	case EGatherAndHuntPuzzlePlaybackEntityKind::Objective:
	default:
		ObjectiveBindings.Add(EntityId, Binding);
		break;
	}
}

void UGatherAndHuntPuzzlePlaybackComponent::RegisterTurtleActor(const int32 TurtleId, AActor* Actor)
{
	RegisterActor(EGatherAndHuntPuzzlePlaybackEntityKind::Turtle, TurtleId, Actor);
}

void UGatherAndHuntPuzzlePlaybackComponent::RegisterObstacleActor(const int32 ObstacleId, AActor* Actor)
{
	RegisterActor(EGatherAndHuntPuzzlePlaybackEntityKind::Obstacle, ObstacleId, Actor);
}

void UGatherAndHuntPuzzlePlaybackComponent::RegisterObjectiveActor(const int32 ObjectiveId, AActor* Actor)
{
	RegisterActor(EGatherAndHuntPuzzlePlaybackEntityKind::Objective, ObjectiveId, Actor);
}

void UGatherAndHuntPuzzlePlaybackComponent::ClearRegisteredActors()
{
	TurtleBindings.Reset();
	ObstacleBindings.Reset();
	ObjectiveBindings.Reset();
}

void UGatherAndHuntPuzzlePlaybackComponent::ApplyBoardSnapshotInstant(const FGatherAndHuntPuzzleBoardSnapshot& BoardSnapshot)
{
	for (const FGatherAndHuntPuzzleTurtleState& TurtleState : BoardSnapshot.Turtles)
	{
		if (FPlaybackBinding* const Binding = TurtleBindings.Find(TurtleState.TurtleId))
		{
			const FVector WorldLocation = GridToWorld(TurtleState.GridPosition);
			const FRotator WorldRotation = RotationFromHeadingPitch(TurtleState.Heading, TurtleState.Pitch);
			Binding->StartLocation = WorldLocation;
			Binding->TargetLocation = WorldLocation;
			Binding->StartRotation = WorldRotation;
			Binding->TargetRotation = WorldRotation;
			Binding->bPendingHideAtTickEnd = false;
			SetActorTransform(Binding->Actor, WorldLocation, WorldRotation, TurtleState.bAlive && !TurtleState.bDelivered);
		}
	}

	for (const FGatherAndHuntPuzzleObstacleState& ObstacleState : BoardSnapshot.Obstacles)
	{
		if (FPlaybackBinding* const Binding = ObstacleBindings.Find(ObstacleState.ObstacleId))
		{
			const FVector WorldLocation = GridToWorld(ObstacleState.GridPosition);
			Binding->StartLocation = WorldLocation;
			Binding->TargetLocation = WorldLocation;
			Binding->bPendingHideAtTickEnd = false;
			SetActorTransform(Binding->Actor, WorldLocation, Binding->TargetRotation, ObstacleState.bAlive);
		}
	}

	for (const FGatherAndHuntPuzzleObjectiveState& ObjectiveState : BoardSnapshot.Objectives)
	{
		if (FPlaybackBinding* const Binding = ObjectiveBindings.Find(ObjectiveState.ObjectiveId))
		{
			const FVector WorldLocation = GridToWorld(ObjectiveState.GridPosition);
			Binding->StartLocation = WorldLocation;
			Binding->TargetLocation = WorldLocation;
			Binding->bPendingHideAtTickEnd = false;
			const bool bVisible = !(bAutoHideClaimedObjectives && ObjectiveState.bClaimed);
			SetActorTransform(Binding->Actor, WorldLocation, Binding->TargetRotation, bVisible);
		}
	}
}

bool UGatherAndHuntPuzzlePlaybackComponent::StartPlayback(
	const FGatherAndHuntPuzzleBoardSnapshot& InitialBoardSnapshot,
	const FGatherAndHuntPuzzleTurnResultView& TurnResultView)
{
	StopPlayback(false);

	ActiveInitialBoardSnapshot = InitialBoardSnapshot;
	ActiveTurnResultView = TurnResultView;
	ApplyBoardSnapshotInstant(InitialBoardSnapshot);

	if (TurnResultView.Ticks.Num() == 0)
	{
		ApplyFinalBoardSnapshot();
		OnPlaybackStarted.Broadcast();
		OnPlaybackFinished.Broadcast();
		return true;
	}

	bIsPlaying = true;
	bIsPaused = false;
	ActiveTickIndex = 0;
	TickElapsedSeconds = 0.0f;
	OnPlaybackStarted.Broadcast();
	BeginTickPlayback(0);
	return true;
}

bool UGatherAndHuntPuzzlePlaybackComponent::StartPlaybackFromSession(UGatherAndHuntPuzzleSession* PuzzleSession)
{
	if (PuzzleSession == nullptr)
	{
		return false;
	}

	return StartPlayback(
		PuzzleSession->GetLastPlaybackStartBoardSnapshot(),
		PuzzleSession->GetLastTurnResultView());
}

void UGatherAndHuntPuzzlePlaybackComponent::StopPlayback(const bool bApplyFinalBoard)
{
	if (bApplyFinalBoard && ActiveTurnResultView.Ticks.Num() > 0)
	{
		ApplyFinalBoardSnapshot();
	}

	const bool bWasPlaying = bIsPlaying;
	bIsPlaying = false;
	bIsPaused = false;
	ActiveTickIndex = INDEX_NONE;
	TickElapsedSeconds = 0.0f;

	if (bWasPlaying)
	{
		OnPlaybackFinished.Broadcast();
	}
}

void UGatherAndHuntPuzzlePlaybackComponent::PausePlayback()
{
	bIsPaused = true;
}

void UGatherAndHuntPuzzlePlaybackComponent::ResumePlayback()
{
	if (bIsPlaying)
	{
		bIsPaused = false;
	}
}

void UGatherAndHuntPuzzlePlaybackComponent::StepToNextTick()
{
	if (!bIsPlaying || ActiveTickIndex == INDEX_NONE)
	{
		return;
	}

	UpdateActorsForInterpolation(1.0f);
	FinishTickPlayback();
}

void UGatherAndHuntPuzzlePlaybackComponent::BeginTickPlayback(const int32 TickIndex)
{
	if (!ActiveTurnResultView.Ticks.IsValidIndex(TickIndex))
	{
		StopPlayback(true);
		return;
	}

	ActiveTickIndex = TickIndex;
	TickElapsedSeconds = 0.0f;

	for (TPair<int32, FPlaybackBinding>& Pair : TurtleBindings)
	{
		Pair.Value.StartLocation = Pair.Value.TargetLocation;
		Pair.Value.StartRotation = Pair.Value.TargetRotation;
		Pair.Value.bPendingHideAtTickEnd = false;
	}
	for (TPair<int32, FPlaybackBinding>& Pair : ObstacleBindings)
	{
		Pair.Value.StartLocation = Pair.Value.TargetLocation;
		Pair.Value.StartRotation = Pair.Value.TargetRotation;
		Pair.Value.bPendingHideAtTickEnd = false;
	}
	for (TPair<int32, FPlaybackBinding>& Pair : ObjectiveBindings)
	{
		Pair.Value.StartLocation = Pair.Value.TargetLocation;
		Pair.Value.StartRotation = Pair.Value.TargetRotation;
		Pair.Value.bPendingHideAtTickEnd = false;
	}

	const FGatherAndHuntPuzzleTickResultView& TickView = ActiveTurnResultView.Ticks[TickIndex];
	for (const FGatherAndHuntPuzzleTickEventView& EventView : TickView.Events)
	{
		switch (EventView.Type)
		{
		case EGatherAndHuntPuzzleEventType::TurtleMoved:
		case EGatherAndHuntPuzzleEventType::TurtleTurned:
		case EGatherAndHuntPuzzleEventType::TurtleBlocked:
		{
			if (FPlaybackBinding* const Binding = TurtleBindings.Find(EventView.PrimaryId))
			{
				const FVector TargetLocation = GridToWorld(EventView.ToPosition);
				const FRotator TargetRotation = RotationFromHeadingPitch(EventView.ToHeading, EventView.ToPitch);
				UpdateBindingTarget(*Binding, TargetLocation, EventAffectsRotation(EventView.Type) ? TargetRotation : Binding->TargetRotation);
			}
			break;
		}
		case EGatherAndHuntPuzzleEventType::ObstacleMoved:
		{
			if (FPlaybackBinding* const Binding = ObstacleBindings.Find(EventView.PrimaryId))
			{
				const FVector TargetLocation = GridToWorld(EventView.ToPosition);
				const FIntVector Delta = EventView.ToPosition - EventView.FromPosition;
				const FRotator TargetRotation = Delta.IsZero()
					? Binding->TargetRotation
					: Delta.Z > 0 ? FRotator(90.0f, 0.0f, 0.0f)
					: Delta.Z < 0 ? FRotator(-90.0f, 0.0f, 0.0f)
					: FRotationMatrix::MakeFromX(FVector(Delta)).Rotator();
				UpdateBindingTarget(*Binding, TargetLocation, TargetRotation);
			}
			break;
		}
		case EGatherAndHuntPuzzleEventType::TurtleDestroyed:
			if (FPlaybackBinding* const Binding = TurtleBindings.Find(EventView.PrimaryId))
			{
				Binding->bPendingHideAtTickEnd = bAutoHideDestroyedActors;
			}
			break;
		case EGatherAndHuntPuzzleEventType::ObstacleDestroyed:
			if (FPlaybackBinding* const Binding = ObstacleBindings.Find(EventView.PrimaryId))
			{
				Binding->bPendingHideAtTickEnd = bAutoHideDestroyedActors;
			}
			break;
		case EGatherAndHuntPuzzleEventType::TurtleDelivered:
			if (FPlaybackBinding* const Binding = TurtleBindings.Find(EventView.PrimaryId))
			{
				Binding->bPendingHideAtTickEnd = bAutoHideDeliveredTurtles;
			}
			break;
		case EGatherAndHuntPuzzleEventType::TurtleClaimedObjective:
			if (FPlaybackBinding* const Binding = ObjectiveBindings.Find(EventView.SecondaryId))
			{
				Binding->bPendingHideAtTickEnd = bAutoHideClaimedObjectives;
			}
			break;
		case EGatherAndHuntPuzzleEventType::TurtleWaited:
		case EGatherAndHuntPuzzleEventType::HunterRetargeted:
		default:
			break;
		}
	}

	OnPlaybackTickStarted.Broadcast(TickView.TickIndex);
	UpdateActorsForInterpolation(0.0f);
}

void UGatherAndHuntPuzzlePlaybackComponent::FinishTickPlayback()
{
	if (!ActiveTurnResultView.Ticks.IsValidIndex(ActiveTickIndex))
	{
		StopPlayback(true);
		return;
	}

	const FGatherAndHuntPuzzleTickResultView& TickView = ActiveTurnResultView.Ticks[ActiveTickIndex];
	UpdateActorsForInterpolation(1.0f);

	for (TPair<int32, FPlaybackBinding>& Pair : TurtleBindings)
	{
		if (Pair.Value.Actor.IsValid() && Pair.Value.bPendingHideAtTickEnd)
		{
			Pair.Value.Actor->SetActorHiddenInGame(true);
			Pair.Value.Actor->SetActorEnableCollision(false);
		}
	}
	for (TPair<int32, FPlaybackBinding>& Pair : ObstacleBindings)
	{
		if (Pair.Value.Actor.IsValid() && Pair.Value.bPendingHideAtTickEnd)
		{
			Pair.Value.Actor->SetActorHiddenInGame(true);
			Pair.Value.Actor->SetActorEnableCollision(false);
		}
	}
	for (TPair<int32, FPlaybackBinding>& Pair : ObjectiveBindings)
	{
		if (Pair.Value.Actor.IsValid() && Pair.Value.bPendingHideAtTickEnd)
		{
			Pair.Value.Actor->SetActorHiddenInGame(true);
			Pair.Value.Actor->SetActorEnableCollision(false);
		}
	}

	OnPlaybackTickFinished.Broadcast(TickView.TickIndex);

	const int32 NextTickIndex = ActiveTickIndex + 1;
	if (ActiveTurnResultView.Ticks.IsValidIndex(NextTickIndex))
	{
		BeginTickPlayback(NextTickIndex);
		return;
	}

	ApplyFinalBoardSnapshot();
	StopPlayback(false);
}

void UGatherAndHuntPuzzlePlaybackComponent::ApplyFinalBoardSnapshot()
{
	ApplyBoardSnapshotInstant(ActiveTurnResultView.FinalBoard);
}

void UGatherAndHuntPuzzlePlaybackComponent::SetActorTransform(
	const TWeakObjectPtr<AActor>& Actor,
	const FVector& Location,
	const FRotator& Rotation,
	const bool bVisible) const
{
	if (!Actor.IsValid())
	{
		return;
	}

	Actor->SetActorLocation(Location);
	Actor->SetActorRotation(Rotation);
	Actor->SetActorHiddenInGame(!bVisible);
	Actor->SetActorEnableCollision(bVisible);
}

void UGatherAndHuntPuzzlePlaybackComponent::UpdateBindingTarget(
	FPlaybackBinding& Binding,
	const FVector& TargetLocation,
	const FRotator& TargetRotation)
{
	Binding.TargetLocation = TargetLocation;
	Binding.TargetRotation = TargetRotation;
}

FVector UGatherAndHuntPuzzlePlaybackComponent::GridToWorld(const FIntVector& GridPosition) const
{
	return FVector(GridPosition) * GridCellSize;
}

FRotator UGatherAndHuntPuzzlePlaybackComponent::RotationFromHeadingPitch(const int32 Heading, const int32 Pitch) const
{
	float Yaw = 0.0f;
	switch (Heading)
	{
	case 0: Yaw = 90.0f; break;   // North +Y
	case 1: Yaw = 0.0f; break;    // East +X
	case 2: Yaw = -90.0f; break;  // South -Y
	case 3: Yaw = 180.0f; break;  // West -X
	default: break;
	}

	float PitchDegrees = 0.0f;
	if (Pitch == 0)
	{
		PitchDegrees = -90.0f;
	}
	else if (Pitch == 2)
	{
		PitchDegrees = 90.0f;
	}

	return FRotator(PitchDegrees, Yaw, 0.0f);
}

UGatherAndHuntPuzzlePlaybackComponent::FPlaybackBinding* UGatherAndHuntPuzzlePlaybackComponent::FindBinding(
	const EGatherAndHuntPuzzlePlaybackEntityKind EntityKind,
	const int32 EntityId)
{
	switch (EntityKind)
	{
	case EGatherAndHuntPuzzlePlaybackEntityKind::Turtle:
		return TurtleBindings.Find(EntityId);
	case EGatherAndHuntPuzzlePlaybackEntityKind::Obstacle:
		return ObstacleBindings.Find(EntityId);
	case EGatherAndHuntPuzzlePlaybackEntityKind::Objective:
	default:
		return ObjectiveBindings.Find(EntityId);
	}
}

const UGatherAndHuntPuzzlePlaybackComponent::FPlaybackBinding* UGatherAndHuntPuzzlePlaybackComponent::FindBinding(
	const EGatherAndHuntPuzzlePlaybackEntityKind EntityKind,
	const int32 EntityId) const
{
	switch (EntityKind)
	{
	case EGatherAndHuntPuzzlePlaybackEntityKind::Turtle:
		return TurtleBindings.Find(EntityId);
	case EGatherAndHuntPuzzlePlaybackEntityKind::Obstacle:
		return ObstacleBindings.Find(EntityId);
	case EGatherAndHuntPuzzlePlaybackEntityKind::Objective:
	default:
		return ObjectiveBindings.Find(EntityId);
	}
}

void UGatherAndHuntPuzzlePlaybackComponent::UpdateActorsForInterpolation(const float Alpha)
{
	for (TPair<int32, FPlaybackBinding>& Pair : TurtleBindings)
	{
		if (!Pair.Value.Actor.IsValid())
		{
			continue;
		}
		const FVector Location = FMath::Lerp(Pair.Value.StartLocation, Pair.Value.TargetLocation, Alpha);
		const FRotator Rotation = FMath::Lerp(Pair.Value.StartRotation, Pair.Value.TargetRotation, Alpha);
		Pair.Value.Actor->SetActorLocation(Location);
		Pair.Value.Actor->SetActorRotation(Rotation);
	}

	for (TPair<int32, FPlaybackBinding>& Pair : ObstacleBindings)
	{
		if (!Pair.Value.Actor.IsValid())
		{
			continue;
		}
		const FVector Location = FMath::Lerp(Pair.Value.StartLocation, Pair.Value.TargetLocation, Alpha);
		const FRotator Rotation = FMath::Lerp(Pair.Value.StartRotation, Pair.Value.TargetRotation, Alpha);
		Pair.Value.Actor->SetActorLocation(Location);
		Pair.Value.Actor->SetActorRotation(Rotation);
	}

	for (TPair<int32, FPlaybackBinding>& Pair : ObjectiveBindings)
	{
		if (!Pair.Value.Actor.IsValid())
		{
			continue;
		}
		const FVector Location = FMath::Lerp(Pair.Value.StartLocation, Pair.Value.TargetLocation, Alpha);
		const FRotator Rotation = FMath::Lerp(Pair.Value.StartRotation, Pair.Value.TargetRotation, Alpha);
		Pair.Value.Actor->SetActorLocation(Location);
		Pair.Value.Actor->SetActorRotation(Rotation);
	}
}
