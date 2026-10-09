#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GatherAndHuntArtPreview.generated.h"

class AGatherAndHuntPuzzlePieceActor;
class UCameraComponent;
class UDirectionalLightComponent;
class UGatherAndHuntPlannerButton;
class UMaterialInstanceDynamic;
class USpringArmComponent;
class UTextBlock;

// The ship from the art settings on a turntable, with an orbit camera and the
// board's lighting, so a skin looks here as it will in a puzzle.
UCLASS(NotPlaceable)
class GATHERANDHUNT_API AGatherAndHuntArtPreviewStage : public AActor
{
	GENERATED_BODY()

public:
	AGatherAndHuntArtPreviewStage();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Skin names from the art settings; empty if no ship mesh is set.
	const TArray<FName>& GetSkinNames() const { return SkinNames; }
	int32 GetSkinIndex() const { return SkinIndex; }
	void SetSkin(int32 Index);

	// Material parameters tried out here apply to every skin until the screen
	// is left. They aren't saved; set the ones to keep on the material instance.
	float GetParameter(FName Name) const;
	void AdjustParameter(FName Name, float Delta, float Min, float Max);

	void OrbitCamera(float DeltaYaw, float DeltaPitch);
	void ZoomCamera(float Steps);

	bool bSpin = true;

private:
	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|ArtPreview")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|ArtPreview")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|ArtPreview")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|ArtPreview")
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|ArtPreview")
	TObjectPtr<UDirectionalLightComponent> FillLight;

	// Shines up from below, so the hull's underside isn't black.
	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|ArtPreview")
	TObjectPtr<UDirectionalLightComponent> UnderLight;

	UPROPERTY(Transient)
	TObjectPtr<AGatherAndHuntPuzzlePieceActor> Ship;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkinMaterial;

	TArray<FName> SkinNames;
	int32 SkinIndex = INDEX_NONE;
	TMap<FName, float> ParameterOverrides;
};

// Controls for the art preview: skin buttons, animation and glow steppers.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntArtPreviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGatherAndHuntArtPreviewWidget(const FObjectInitializer& ObjectInitializer);

	// Call before adding to the viewport.
	void Bind(AGatherAndHuntArtPreviewStage* InStage);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildTree();
	void Refresh();
	void OnSkin(int32 Index);
	void OnAction(int32 Action);

	UPROPERTY(Transient) TObjectPtr<AGatherAndHuntArtPreviewStage> Stage;
	UPROPERTY(Transient) TArray<TObjectPtr<UGatherAndHuntPlannerButton>> SkinButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ParameterTexts;
	UPROPERTY(Transient) TObjectPtr<UGatherAndHuntPlannerButton> SpinButton;

	bool bOrbiting = false;
};

// Spawns the stage, takes its camera and puts the controls on screen.
UCLASS()
class GATHERANDHUNT_API AGatherAndHuntArtPreviewHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AGatherAndHuntArtPreviewStage> Stage;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntArtPreviewWidget> Widget;
};

// GameMode for the art preview: no pawn, just the preview HUD.
UCLASS()
class GATHERANDHUNT_API AGatherAndHuntArtPreviewGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGatherAndHuntArtPreviewGameMode();
};
