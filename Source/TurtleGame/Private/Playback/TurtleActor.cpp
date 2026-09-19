#include "Playback/TurtleActor.h"

ATurtleActor::ATurtleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;
}

void ATurtleActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector CurLoc = GetActorLocation();
	const FRotator CurRot = GetActorRotation();
	const FVector NewLoc = FMath::VInterpTo(CurLoc, TargetLocation, DeltaSeconds, MoveInterpSpeed);
	const FRotator NewRot = FMath::RInterpTo(CurRot, TargetRotation, DeltaSeconds, RotateInterpSpeed);
	SetActorLocationAndRotation(NewLoc, NewRot);
}

void ATurtleActor::SetTargetPose(const FVector& InTargetLocation, const FRotator& InTargetRotation)
{
	TargetLocation = InTargetLocation;
	TargetRotation = InTargetRotation;
}

void ATurtleActor::SnapToPose(const FVector& InTargetLocation, const FRotator& InTargetRotation)
{
	TargetLocation = InTargetLocation;
	TargetRotation = InTargetRotation;
	SetActorLocationAndRotation(InTargetLocation, InTargetRotation);
}

void ATurtleActor::SetVisualVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
}
