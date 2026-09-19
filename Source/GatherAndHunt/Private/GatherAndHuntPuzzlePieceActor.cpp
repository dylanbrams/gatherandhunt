#include "GatherAndHuntPuzzlePieceActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

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

void AGatherAndHuntPuzzlePieceActor::FaceLabelTowards(const FVector& ViewLocation)
{
	const FVector ToView = ViewLocation - LabelComponent->GetComponentLocation();
	// Text renders along its +X, so point +X at the camera.
	LabelComponent->SetWorldRotation(ToView.Rotation());
}
