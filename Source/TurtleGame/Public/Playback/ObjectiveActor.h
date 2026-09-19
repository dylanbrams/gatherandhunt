#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectiveActor.generated.h"

// Dumb visual actor for an objective (a stationary point pickup). Hidden when claimed.
UCLASS(Blueprintable)
class TURTLEGAME_API AObjectiveActor : public AActor
{
	GENERATED_BODY()

public:
	AObjectiveActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	int32 ObjectiveId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	int32 PointValue = 0;

	UFUNCTION(BlueprintCallable, Category = "Objective")
	void SnapToLocation(const FVector& InLocation);

	UFUNCTION(BlueprintCallable, Category = "Objective")
	void SetVisualVisible(bool bVisible);
};
