#include "GatherAndHuntArtPreview.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GatherAndHuntArtSettings.h"
#include "GatherAndHuntFrontEnd.h"
#include "GatherAndHuntPlayerOptions.h"
#include "GatherAndHuntPuzzlePieceActor.h"
#include "GatherAndHuntWidgetBuilder.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace GatherAndHuntUI;

namespace
{
	// The ship's longest side on the stage, in world units.
	constexpr float PreviewShipSize = 400.0f;

	constexpr float SpinDegreesPerSecond = 25.0f;

	// Matches the board view, so brightness here is what a puzzle shows.
	constexpr float ExposureEV100 = 2.0f;

	// A material parameter with -/+ buttons.
	struct FPreviewParameter
	{
		const TCHAR* Label;
		FName Name;
		float Step;
		float Min;
		float Max;
	};

	const FPreviewParameter PreviewParameters[] = {
		{TEXT("Cycle seconds"), TEXT("CycleSeconds"), 0.25f, 0.25f, 20.0f},
		{TEXT("Blend"), TEXT("Blend"), 0.1f, 0.0f, 1.0f},
		{TEXT("Light strength"), TEXT("RunningLightStrength"), 1.0f, 0.0f, 50.0f},
		{TEXT("Light seconds"), TEXT("RunningLightSeconds"), 0.25f, 0.5f, 10.0f},
	};

	// Action payloads: two per parameter (down, up), then these.
	constexpr int32 ActionSpin = 100;
	constexpr int32 ActionMenu = 101;
}

// --- Stage ---------------------------------------------------------------------

AGatherAndHuntArtPreviewStage::AGatherAndHuntArtPreviewStage()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Root);
	CameraArm->bDoCollisionTest = false;
	CameraArm->SetRelativeRotation(FRotator(-25.0f, -135.0f, 0.0f));
	CameraArm->TargetArmLength = PreviewShipSize * 1.6f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);
	Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Camera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	Camera->PostProcessSettings.AutoExposureMinBrightness = ExposureEV100;
	Camera->PostProcessSettings.AutoExposureMaxBrightness = ExposureEV100;

	auto MakeLight = [this](const TCHAR* Name, const FRotator& Rotation, const float Intensity)
	{
		UDirectionalLightComponent* const Light = CreateDefaultSubobject<UDirectionalLightComponent>(Name);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetupAttachment(Root);
		Light->SetRelativeRotation(Rotation);
		Light->SetIntensity(Intensity);
		Light->SetCastShadows(false);
		return Light;
	};
	// Same key, fill and under light as the board view's fallback lighting.
	KeyLight = MakeLight(TEXT("KeyLight"), FRotator(-50.0f, -35.0f, 0.0f), 6.0f);
	FillLight = MakeLight(TEXT("FillLight"), FRotator(-30.0f, 145.0f, 0.0f), 2.5f);
	UnderLight = MakeLight(TEXT("UnderLight"), FRotator(60.0f, 20.0f, 0.0f), 2.5f);
	KeyLight->ForwardShadingPriority = 1;
}

void AGatherAndHuntArtPreviewStage::BeginPlay()
{
	Super::BeginPlay();

	const UGatherAndHuntArtSettings* const Art = GetDefault<UGatherAndHuntArtSettings>();
	UStaticMesh* const Mesh = Art->ShipMesh.LoadSynchronous();
	if (Mesh == nullptr)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Ship = GetWorld()->SpawnActor<AGatherAndHuntPuzzlePieceActor>(Params);
	Ship->SetupArt(Mesh, nullptr, Art->ShipMeshRotation, PreviewShipSize, FString());
	Art->AddShipGlows(Ship, PreviewShipSize);

	for (const FGatherAndHuntShipSkin& Skin : Art->ShipSkins)
	{
		SkinNames.Add(Skin.Name);
	}
	SetSkin(0);
}

void AGatherAndHuntArtPreviewStage::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bSpin && Ship != nullptr)
	{
		Ship->AddActorWorldRotation(FRotator(0.0f, SpinDegreesPerSecond * DeltaSeconds, 0.0f));
	}
	if (Ship != nullptr)
	{
		Ship->UpdateForView(Camera->GetComponentLocation());
	}
}

void AGatherAndHuntArtPreviewStage::SetSkin(const int32 Index)
{
	const TArray<FGatherAndHuntShipSkin>& Skins = GetDefault<UGatherAndHuntArtSettings>()->ShipSkins;
	if (Ship == nullptr || !Skins.IsValidIndex(Index))
	{
		return;
	}
	UMaterialInterface* const Material = Skins[Index].Material.LoadSynchronous();
	if (Material == nullptr)
	{
		return;
	}
	SkinIndex = Index;
	SkinMaterial = UMaterialInstanceDynamic::Create(Material, this);
	for (const TPair<FName, float>& Override : ParameterOverrides)
	{
		SkinMaterial->SetScalarParameterValue(Override.Key, Override.Value);
	}
	Ship->GetMeshComponent()->SetMaterial(0, SkinMaterial);
}

float AGatherAndHuntArtPreviewStage::GetParameter(const FName Name) const
{
	return SkinMaterial != nullptr ? SkinMaterial->K2_GetScalarParameterValue(Name) : 0.0f;
}

void AGatherAndHuntArtPreviewStage::AdjustParameter(const FName Name, const float Delta, const float Min, const float Max)
{
	if (SkinMaterial == nullptr)
	{
		return;
	}
	const float Value = FMath::Clamp(GetParameter(Name) + Delta, Min, Max);
	ParameterOverrides.Add(Name, Value);
	SkinMaterial->SetScalarParameterValue(Name, Value);
}

void AGatherAndHuntArtPreviewStage::OrbitCamera(const float DeltaYaw, const float DeltaPitch)
{
	FRotator Rotation = CameraArm->GetRelativeRotation();
	Rotation.Yaw += DeltaYaw;
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + DeltaPitch, -89.0f, 89.0f);
	CameraArm->SetRelativeRotation(Rotation);
}

void AGatherAndHuntArtPreviewStage::ZoomCamera(const float Steps)
{
	CameraArm->TargetArmLength = FMath::Clamp(CameraArm->TargetArmLength * FMath::Pow(0.9f, Steps), PreviewShipSize * 0.6f, PreviewShipSize * 60.0f);
}

// --- Widget --------------------------------------------------------------------

UGatherAndHuntArtPreviewWidget::UGatherAndHuntArtPreviewWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UGatherAndHuntArtPreviewWidget::Bind(AGatherAndHuntArtPreviewStage* InStage)
{
	Stage = InStage;
}

TSharedRef<SWidget> UGatherAndHuntArtPreviewWidget::RebuildWidget()
{
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildTree();
		Refresh();
	}
	return Super::RebuildWidget();
}

void UGatherAndHuntArtPreviewWidget::BuildTree()
{
	// Transparent full-screen backdrop: catches mouse drags/wheel on empty space
	// for the orbit camera.
	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(FLinearColor::Transparent);
	Backdrop->SetPadding(FMargin(0.0f));
	Backdrop->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Backdrop;

	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Backdrop->SetContent(Canvas);

	auto Action = [this](const FString& Label, const int32 InAction)
	{
		return MakeButton(WidgetTree, this, Label, InAction, &UGatherAndHuntArtPreviewWidget::OnAction);
	};

	UVerticalBox* const Box = WidgetTree->ConstructWidget<UVerticalBox>();
	AddRow(Box, MakeText(WidgetTree, TEXT("ART PREVIEW"), 16), 0.0f);

	AddRow(Box, MakeText(WidgetTree, TEXT("SKIN (Left / Right)"), 10, DimTextColor), 12.0f);
	const TArray<FName> SkinNames = Stage != nullptr ? Stage->GetSkinNames() : TArray<FName>();
	for (int32 Index = 0; Index < SkinNames.Num(); ++Index)
	{
		UGatherAndHuntPlannerButton* const Button = MakeButton(WidgetTree, this, SkinNames[Index].ToString(), Index, &UGatherAndHuntArtPreviewWidget::OnSkin);
		AlignContentLeft(Button);
		AddRow(Box, Button);
		SkinButtons.Add(Button);
	}
	if (SkinNames.Num() == 0)
	{
		UTextBlock* const Missing = MakeText(WidgetTree,
			TEXT("No ship mesh or skins are set. See Project Settings > Game > Gather And Hunt Art."), 11, WarningColor);
		Missing->SetAutoWrapText(true);
		AddRow(Box, Missing);
	}

	AddRow(Box, MakeText(WidgetTree, TEXT("MATERIAL (not saved)"), 10, DimTextColor), 12.0f);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PreviewParameters); ++Index)
	{
		UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		AddCell(Row, Action(TEXT(" - "), Index * 2));
		AddCell(Row, Action(TEXT(" + "), Index * 2 + 1), 8.0f);
		UTextBlock* const Text = MakeText(WidgetTree, TEXT(""));
		AddCell(Row, Text);
		ParameterTexts.Add(Text);
		AddRow(Box, Row);
	}

	SpinButton = Action(TEXT(""), ActionSpin);
	AddRow(Box, SpinButton, 12.0f);
	AddRow(Box, MakeText(WidgetTree, TEXT("Drag to orbit, wheel to zoom."), 10, DimTextColor), 6.0f);
	AddRow(Box, Action(TEXT("Main menu (Esc)"), ActionMenu), 12.0f);

	Place(Canvas, SizeTo(WidgetTree, MakePanel(WidgetTree, Box), 260.0f), FVector2D(0.0f, 0.0f), FVector2D(16.0f, 16.0f));
	// The player's own options, here because this is where their effect is easiest to judge.
	Place(Canvas, WidgetTree->ConstructWidget<UGatherAndHuntOptionsWidget>(), FVector2D(1.0f, 0.0f), FVector2D(-16.0f, 16.0f));
}

void UGatherAndHuntArtPreviewWidget::Refresh()
{
	if (Stage == nullptr)
	{
		return;
	}
	for (int32 Index = 0; Index < SkinButtons.Num(); ++Index)
	{
		SkinButtons[Index]->SetBackgroundColor(Index == Stage->GetSkinIndex() ? SelectedColor : FLinearColor::White);
	}
	for (int32 Index = 0; Index < ParameterTexts.Num(); ++Index)
	{
		const FPreviewParameter& Parameter = PreviewParameters[Index];
		ParameterTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s  %.2f"), Parameter.Label, Stage->GetParameter(Parameter.Name))));
	}
	if (UTextBlock* const SpinLabel = Cast<UTextBlock>(SpinButton->GetContent()))
	{
		SpinLabel->SetText(FText::FromString(Stage->bSpin ? TEXT("Turntable: on (Space)") : TEXT("Turntable: off (Space)")));
	}
}

void UGatherAndHuntArtPreviewWidget::OnSkin(const int32 Index)
{
	if (Stage != nullptr)
	{
		Stage->SetSkin(Index);
		Refresh();
	}
}

void UGatherAndHuntArtPreviewWidget::OnAction(const int32 InAction)
{
	if (InAction == ActionMenu)
	{
		UGatherAndHuntFrontEnd::OpenMainMenu(this);
		return;
	}
	if (Stage == nullptr)
	{
		return;
	}
	if (InAction == ActionSpin)
	{
		Stage->bSpin = !Stage->bSpin;
	}
	else if (InAction >= 0 && InAction < 2 * static_cast<int32>(UE_ARRAY_COUNT(PreviewParameters)))
	{
		const FPreviewParameter& Parameter = PreviewParameters[InAction / 2];
		Stage->AdjustParameter(Parameter.Name, (InAction % 2 == 0 ? -1.0f : 1.0f) * Parameter.Step, Parameter.Min, Parameter.Max);
	}
	Refresh();
}

FReply UGatherAndHuntArtPreviewWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		OnAction(ActionMenu);
		return FReply::Handled();
	}
	if (Key == EKeys::SpaceBar)
	{
		OnAction(ActionSpin);
		return FReply::Handled();
	}
	if ((Key == EKeys::Left || Key == EKeys::Right) && Stage != nullptr && Stage->GetSkinNames().Num() > 0)
	{
		const int32 Count = Stage->GetSkinNames().Num();
		OnSkin((Stage->GetSkinIndex() + (Key == EKeys::Right ? 1 : Count - 1)) % Count);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UGatherAndHuntArtPreviewWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	bOrbiting = true;
	return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget());
}

FReply UGatherAndHuntArtPreviewWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bOrbiting)
	{
		bOrbiting = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UGatherAndHuntArtPreviewWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bOrbiting && Stage != nullptr)
	{
		const FVector2D Delta = InMouseEvent.GetCursorDelta();
		Stage->OrbitCamera(Delta.X * 0.3f, -Delta.Y * 0.3f);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UGatherAndHuntArtPreviewWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Stage != nullptr)
	{
		Stage->ZoomCamera(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

// --- HUD and GameMode ----------------------------------------------------------

void AGatherAndHuntArtPreviewHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* const PC = GetOwningPlayerController();
	if (PC == nullptr)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Stage = GetWorld()->SpawnActor<AGatherAndHuntArtPreviewStage>(Params);

	Widget = CreateWidget<UGatherAndHuntArtPreviewWidget>(PC, UGatherAndHuntArtPreviewWidget::StaticClass());
	Widget->Bind(Stage);
	Widget->AddToViewport();

	// The stage owns the camera; keep possession from stealing it back.
	PC->bAutoManageActiveCameraTarget = false;
	PC->SetViewTarget(Stage);
	PC->SetShowMouseCursor(true);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
}

AGatherAndHuntArtPreviewGameMode::AGatherAndHuntArtPreviewGameMode()
{
	HUDClass = AGatherAndHuntArtPreviewHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
