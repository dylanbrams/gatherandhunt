#include "GatherAndHuntPuzzlePieceActor.h"

#include "Components/MaterialBillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GatherAndHuntPlayerOptions.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// A glow sprite starts to show when its minimum on-screen size reaches this
	// share of its close-up size.
	constexpr float GlowFadeStart = 0.35f;

	// A one-sided glow crossfades with the other side's over this much of the
	// turn either side of edge-on (as the cosine of the angle).
	constexpr float GlowSideBlend = 0.2f;
}

AGatherAndHuntPuzzlePieceActor::AGatherAndHuntPuzzlePieceActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(Root);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);

	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComponent->SetupAttachment(Root);
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetVerticalAlignment(EVRTA_TextCenter);
	LabelComponent->SetTextRenderColor(FColor::White);
	LabelComponent->SetWorldSize(60.0f);
	// Labels stay upright; only yaw/pitch toward the camera are applied later.
	LabelComponent->SetUsingAbsoluteRotation(true);
}

void AGatherAndHuntPuzzlePieceActor::Setup(
	UStaticMesh* Mesh,
	UMaterialInterface* TintableMaterial,
	const FLinearColor& Color,
	const FRotator& MeshRotation,
	const float MeshScale,
	const FString& Label)
{
	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->SetRelativeRotation(MeshRotation);
	MeshComponent->SetRelativeScale3D(FVector(MeshScale));

	if (TintableMaterial != nullptr)
	{
		UMaterialInstanceDynamic* const Tinted = UMaterialInstanceDynamic::Create(TintableMaterial, this);
		Tinted->SetVectorParameterValue(TEXT("Color"), Color);
		MeshComponent->SetMaterial(0, Tinted);
	}

	LabelComponent->SetText(FText::FromString(Label));
	LabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f * MeshScale * 0.5f + 25.0f));
}

void AGatherAndHuntPuzzlePieceActor::SetupArt(
	UStaticMesh* Mesh,
	UMaterialInterface* Material,
	const FRotator& MeshRotation,
	const float FitSize,
	const FString& Label)
{
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const float Scale = FitSize / FMath::Max(2.0f * static_cast<float>(Bounds.BoxExtent.GetMax()), KINDA_SMALL_NUMBER);

	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->SetRelativeRotation(MeshRotation);
	MeshComponent->SetRelativeScale3D(FVector(Scale));
	// Scans rarely have their pivot in the middle; turn about the bounds' centre.
	MeshComponent->SetRelativeLocation(-MeshRotation.RotateVector(Bounds.Origin * Scale));
	MeshComponent->SetMaterial(0, Material);

	LabelComponent->SetText(FText::FromString(Label));
	LabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, FitSize * 0.5f + 25.0f));
}

void AGatherAndHuntPuzzlePieceActor::SetLabel(const FString& Label, const FColor& Color)
{
	LabelComponent->SetText(FText::FromString(Label));
	LabelComponent->SetTextRenderColor(Color);
}

void AGatherAndHuntPuzzlePieceActor::AddGlow(
	UMaterialInterface* Material,
	const FVector& MeshLocation,
	const FVector& MeshFacing,
	const float WorldSize,
	const float MinScreenSize,
	const bool bShowCloseUp)
{
	UMaterialBillboardComponent* const Glow = NewObject<UMaterialBillboardComponent>(this);
	// Attached to the mesh, so MeshLocation is in the mesh's space and follows its fit.
	Glow->SetupAttachment(MeshComponent);
	Glow->SetRelativeLocation(MeshLocation);
	Glow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glow->SetCastShadow(false);
	// Its own instance, so UpdateForView can fade it.
	UMaterialInstanceDynamic* const Faded = UMaterialInstanceDynamic::Create(Material, this);
	Faded->SetScalarParameterValue(TEXT("Fade"), 0.0f);
	Glow->AddElement(Faded, nullptr, false, WorldSize * 0.5f, WorldSize * 0.5f, nullptr);
	Glow->RegisterComponent();
	AddInstanceComponent(Glow);
	Glows.Add(Glow);
	GlowShapes.Add({WorldSize, MinScreenSize, MeshFacing, bShowCloseUp});
}

float AGatherAndHuntPuzzlePieceActor::GlowFade(const FGlowShape& Shape, const FVector& GlowLocation, const FVector& ViewLocation) const
{
	// Close up the hull's own glow shows where the light is, and a sprite on
	// top of it looks stuck on. Bring it in as the camera pulls back, reaching
	// full once it would be at its minimum on-screen size. The player's
	// distance option moves how far back that is.
	float Fade = 1.0f;
	if (!Shape.bShowCloseUp)
	{
		const float Distance = static_cast<float>(FVector::Dist(ViewLocation, GlowLocation)) / UGatherAndHuntPlayerOptions::Get()->FarLightDistance;
		Fade = FMath::SmoothStep(GlowFadeStart, 1.0f, Shape.MinScreenSize * Distance / FMath::Max(Shape.WorldSize, KINDA_SMALL_NUMBER));
	}
	if (!Shape.MeshFacing.IsNearlyZero())
	{
		// Only for a camera on this glow's side of the ship, crossfading
		// with the other side's glows through the edge-on view.
		const FVector Facing = MeshComponent->GetComponentTransform().TransformVectorNoScale(Shape.MeshFacing).GetSafeNormal();
		const FVector ToCamera = (ViewLocation - GlowLocation).GetSafeNormal();
		Fade *= FMath::SmoothStep(-GlowSideBlend, GlowSideBlend, static_cast<float>(FVector::DotProduct(Facing, ToCamera)));
	}
	return Fade;
}

void AGatherAndHuntPuzzlePieceActor::UpdateForView(const FVector& ViewLocation)
{
	const FVector ToView = ViewLocation - LabelComponent->GetComponentLocation();
	// Text renders along its +X, so point +X at the camera.
	LabelComponent->SetWorldRotation(ToView.Rotation());

	const UGatherAndHuntPlayerOptions* const Options = UGatherAndHuntPlayerOptions::Get();
	for (int32 Index = 0; Index < Glows.Num(); ++Index)
	{
		UMaterialBillboardComponent* const Glow = Glows[Index];
		const FGlowShape& Shape = GlowShapes[Index];
		const float Distance = static_cast<float>(FVector::Dist(ViewLocation, Glow->GetComponentLocation()));
		const float HalfSize = FMath::Max(Shape.WorldSize, Shape.MinScreenSize * Options->FarLightSize * Distance) * 0.5f;

		if (UMaterialInstanceDynamic* const Faded = Cast<UMaterialInstanceDynamic>(Glow->GetMaterial(0)))
		{
			Faded->SetScalarParameterValue(TEXT("Fade"), GlowFade(Shape, Glow->GetComponentLocation(), ViewLocation));
			Faded->SetScalarParameterValue(TEXT("PulseRate"), Options->FarLightBlinkSpeed);
		}

		// Changing a billboard's size rebuilds its render state, so skip changes too small to see.
		if (Glow->Elements.Num() > 0 && !FMath::IsNearlyEqual(Glow->Elements[0].BaseSizeX, HalfSize, HalfSize * 0.02f))
		{
			Glow->Elements[0].BaseSizeX = HalfSize;
			Glow->Elements[0].BaseSizeY = HalfSize;
			Glow->MarkRenderStateDirty();
		}
	}
}
