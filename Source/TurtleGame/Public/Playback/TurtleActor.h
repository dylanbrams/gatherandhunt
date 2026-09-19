#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TurtleActor.generated.h"

// Dumb visual actor for a single turtle. Holds target position/rotation and
// interpolates each frame; never queries simulation state directly. Owned and
// driven by UTurtlePlaybackController.
UCLASS(Blueprintable)
class TURTLEGAME_API ATurtleActor : public AActor
{
	GENERATED_BODY()

public:
	ATurtleActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turtle")
	int32 TurtleId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turtle")
	int32 OwnerSlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turtle")
	float MoveInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turtle")
	float RotateInterpSpeed = 12.0f;

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Turtle")
	void SetTargetPose(const FVector& InTargetLocation, const FRotator& InTargetRotation);

	UFUNCTION(BlueprintCallable, Category = "Turtle")
	void SnapToPose(const FVector& InTargetLocation, const FRotator& InTargetRotation);

	UFUNCTION(BlueprintCallable, Category = "Turtle")
	void SetVisualVisible(bool bVisible);

private:
	FVector  TargetLocation = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;
};
