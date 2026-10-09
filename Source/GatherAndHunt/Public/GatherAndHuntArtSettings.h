#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntArtSettings.generated.h"

class AGatherAndHuntPuzzlePieceActor;
class UMaterialInterface;
class UStaticMesh;

// One look for the ship mesh: a material (usually an instance of
// M_ShipFrameCycle with its four frame textures set).
USTRUCT()
struct GATHERANDHUNT_API FGatherAndHuntShipSkin
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	FName Name;

	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	TSoftObjectPtr<UMaterialInterface> Material;
};

// A soft glow fixed to the ship, drawn facing the camera on top of everything.
// It never shrinks below MinScreenSize, so from far away the glows still show
// which end of the ship is which.
USTRUCT()
struct GATHERANDHUNT_API FGatherAndHuntShipGlow
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	FName Name;

	// Usually an instance of M_ShipGlow.
	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	TSoftObjectPtr<UMaterialInterface> Material;

	// Where on the ship, in the mesh's own units and axes.
	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	FVector MeshLocation = FVector::ZeroVector;

	// The side of the ship this glow belongs to, as a direction in the mesh's
	// axes: it shows only to a camera on that side. Zero shows it from all round.
	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	FVector Facing = FVector::ZeroVector;

	// Most glows stand in for a glowing part of the hull and stay hidden close
	// up, where the hull shows it better. A glow that is the light itself (a
	// wingtip strobe) is drawn at every distance.
	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	bool bShowCloseUp = false;

	// Diameter close up, as a fraction of the ship's longest side.
	UPROPERTY(Config, EditAnywhere, Category = "Ship", meta = (ClampMin = "0.0"))
	float Size = 0.2f;

	// Smallest diameter, as a fraction of the distance to the camera. 0.02 is
	// about 1% of the screen's width.
	UPROPERTY(Config, EditAnywhere, Category = "Ship", meta = (ClampMin = "0.0"))
	float MinScreenSize = 0.02f;
};

// Which art the board uses, edited under Project Settings > Game > Gather And
// Hunt Art and stored in Config/DefaultGame.ini. Anything left unset falls back
// to the engine basic shapes.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Gather And Hunt Art"))
class GATHERANDHUNT_API UGatherAndHuntArtSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	TSoftObjectPtr<UStaticMesh> ShipMesh;

	// Turns the mesh so its nose points along +X and its top along +Z.
	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	FRotator ShipMeshRotation = FRotator::ZeroRotator;

	// The ship's longest side, as a fraction of a board cell.
	UPROPERTY(Config, EditAnywhere, Category = "Ship", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float ShipCellFill = 0.9f;

	// Every skin; the art preview steps through them in this order.
	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	TArray<FGatherAndHuntShipSkin> ShipSkins;

	// Nose, tail and wingtip lights: what shows a ship's heading and which
	// way up it is from a distance.
	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	TArray<FGatherAndHuntShipGlow> ShipGlows;

	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	FName PlayerSkin;

	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	FName AllySkin;

	UPROPERTY(Config, EditAnywhere, Category = "Ship")
	FName OpponentSkin;

	// Adds every glow in ShipGlows to a piece set up with the ship mesh at FitSize.
	void AddShipGlows(AGatherAndHuntPuzzlePieceActor* Piece, float FitSize) const;

	// nullptr if the role's skin is unset or missing.
	UMaterialInterface* LoadSkinForRole(EGatherAndHuntTurtleRole TurtleRole) const;
};
