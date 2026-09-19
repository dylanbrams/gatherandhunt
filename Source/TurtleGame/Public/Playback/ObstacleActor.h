#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObstacleActor.generated.h"

// Dumb visual actor for a static obstacle or hunter. Same model as ATurtleActor
// (target pose + interp); kind is purely cosmetic for choice of mesh/effects.
UCLASS(Blueprintable)
class TURTLEGAME_API AObstacleActor : public AActor
{
	GENERATED_BODY()

public:
	AObstacleActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Obstacle")
	int32 ObstacleId = INDEX_NONE;

	// 0 = Static, 1 = Wandering, 2 = Hunter (mirrors turtlesim::EObstacleKind).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Obstacle")
	int32 Kind = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle")
	float MoveInterpSpeed = 6.0f;

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	void SetTargetLocation(const FVector& InTargetLocation);

	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	void SnapToLocation(const FVector& InTargetLocation);

	UFUNCTION(BlueprintCallable, Category = "Obstacle")
	void SetVisualVisible(bool bVisible);

private:
	FVector TargetLocation = FVector::ZeroVector;
};
