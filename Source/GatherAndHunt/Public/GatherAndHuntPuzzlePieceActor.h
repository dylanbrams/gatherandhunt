#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GatherAndHuntPuzzlePieceActor.generated.h"

class UMaterialBillboardComponent;
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

	// Real art: the mesh keeps Material (or its own, if nullptr) untinted, is
	// recentred on its bounds and scaled so its longest side is FitSize.
	void SetupArt(UStaticMesh* Mesh, UMaterialInterface* Material, const FRotator& MeshRotation, float FitSize, const FString& Label);

	// A camera-facing glow at a point on the mesh (in the mesh's own space).
	// WorldSize is its diameter close up; it is never drawn smaller than
	// MinScreenSize times its distance from the camera. A non-zero MeshFacing
	// (also in the mesh's space) shows it only to a camera on that side. It
	// fades out close up unless bShowCloseUp.
	void AddGlow(UMaterialInterface* Material, const FVector& MeshLocation, const FVector& MeshFacing, float WorldSize, float MinScreenSize, bool bShowCloseUp);

	// Call each frame with the camera's location: turns the label to face it
	// and keeps the glows at their minimum on-screen size.
	void UpdateForView(const FVector& ViewLocation);

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	void SetLabel(const FString& Label, const FColor& Color = FColor::White);

private:
	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|Piece")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|Piece")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|Piece")
	TObjectPtr<UTextRenderComponent> LabelComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialBillboardComponent>> Glows;

	struct FGlowShape
	{
		float WorldSize;
		float MinScreenSize;
		FVector MeshFacing;
		bool bShowCloseUp;
	};

	// One per entry in Glows.
	TArray<FGlowShape> GlowShapes;

	// How much of a glow shows from ViewLocation: 0 close up (unless it
	// shows close up), 1 from far away, and 0 from the wrong side of the ship.
	float GlowFade(const FGlowShape& Shape, const FVector& GlowLocation, const FVector& ViewLocation) const;
};
