#include "Playback/ObjectiveActor.h"

AObjectiveActor::AObjectiveActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void AObjectiveActor::SnapToLocation(const FVector& InLocation)
{
	SetActorLocation(InLocation);
}

void AObjectiveActor::SetVisualVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
}
