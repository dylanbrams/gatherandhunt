#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GatherAndHuntPuzzlePieceActor.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

// Placeholder visual for one board entity: a tinted engine basic shape plus a
// text label. Purely presentational; the playback component moves it.
UCLASS(NotPlaceable)
class GATHERANDHUNT_API AGatherAndHuntPuzzlePieceActor : public AActor
{
	GENERATED_BODY()

public:
	AGatherAndHuntPuzzlePieceActor();

	// MeshRotation is relative to the actor, so e.g. a cone can be turned to
	// point along the actor's forward (+X) axis. TintableMaterial must expose a
	// "Color" vector parameter.
	void Setup(UStaticMesh* Mesh, UMaterialInterface* TintableMaterial, const FLinearColor& Color, const FRotator& MeshRotation, float MeshScale, const FString& Label);

	void FaceLabelTowards(const FVector& ViewLocation);

private:
	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|Piece")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|Piece")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|Piece")
	TObjectPtr<UTextRenderComponent> LabelComponent;
};
