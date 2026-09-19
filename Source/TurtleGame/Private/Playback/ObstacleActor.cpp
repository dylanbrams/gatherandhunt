#include "Playback/ObstacleActor.h"

AObstacleActor::AObstacleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;
}

void AObstacleActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FVector CurLoc = GetActorLocation();
	const FVector NewLoc = FMath::VInterpTo(CurLoc, TargetLocation, DeltaSeconds, MoveInterpSpeed);
	SetActorLocation(NewLoc);
}

void AObstacleActor::SetTargetLocation(const FVector& InTargetLocation) { TargetLocation = InTargetLocation; }
void AObstacleActor::SnapToLocation(const FVector& InTargetLocation)
{
	TargetLocation = InTargetLocation;
	SetActorLocation(InTargetLocation);
}
void AObstacleActor::SetVisualVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
}
