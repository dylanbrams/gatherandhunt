#include "GatherAndHuntPuzzleBoardView.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "GatherAndHuntPuzzlePieceActor.h"
#include "GatherAndHuntPuzzlePlaybackComponent.h"
#include "GatherAndHuntPuzzleSession.h"
#include "GatherAndHuntTurnPlanner.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Engine basic shapes are 100 units across.
	constexpr float BasicShapeSize = 100.0f;

	// Camera exposure, matched to the fallback lights' intensity.
	constexpr float ExposureEV100 = 2.0f;

	const FLinearColor PlayerTurtleColor(0.15f, 0.75f, 0.3f);
	const FLinearColor OpponentTurtleColor(0.8f, 0.1f, 0.6f);
	const FLinearColor StaticObstacleColor(0.35f, 0.35f, 0.38f);
	const FLinearColor HazardColor(1.0f, 0.45f, 0.0f);
	const FLinearColor HunterColor(0.9f, 0.05f, 0.05f);
	const FLinearColor ObjectiveColor(1.0f, 0.8f, 0.0f);

	// Obstacle kinds as reported in FGatherAndHuntPuzzleObstacleState::ObstacleKind.
	constexpr int32 ObstacleKindWandering = 1;
	constexpr int32 ObstacleKindHunter = 2;

	FVector ForwardFromHeadingPitch(const int32 Heading, const int32 Pitch)
	{
		if (Pitch == 0) return FVector(0.0f, 0.0f, -1.0f);
		if (Pitch == 2) return FVector(0.0f, 0.0f, 1.0f);
		switch (Heading)
		{
		case 0: return FVector(0.0f, 1.0f, 0.0f);
		case 1: return FVector(1.0f, 0.0f, 0.0f);
		case 2: return FVector(0.0f, -1.0f, 0.0f);
		default: return FVector(-1.0f, 0.0f, 0.0f);
		}
	}
}

AGatherAndHuntPuzzleBoardView::AGatherAndHuntPuzzleBoardView()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	CubeMesh = CubeFinder.Object;
	SphereMesh = SphereFinder.Object;
	ConeMesh = ConeFinder.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	TintableMaterial = MaterialFinder.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Root);
	CameraArm->bDoCollisionTest = false;
	CameraArm->SetRelativeRotation(FRotator(-40.0f, -135.0f, 0.0f));
	CameraArm->TargetArmLength = 2500.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);
	// Fixed exposure: in an empty (black) map eye adaptation otherwise ramps up
	// until every piece saturates to white.
	Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Camera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	Camera->PostProcessSettings.AutoExposureMinBrightness = ExposureEV100;
	Camera->PostProcessSettings.AutoExposureMaxBrightness = ExposureEV100;

	Playback = CreateDefaultSubobject<UGatherAndHuntPuzzlePlaybackComponent>(TEXT("Playback"));

	BoundsMesh = CreateOverlayMesh(TEXT("BoundsMesh"), CubeMesh);
	SelectedPathMesh = CreateOverlayMesh(TEXT("SelectedPathMesh"), CubeMesh);
	OtherPathMesh = CreateOverlayMesh(TEXT("OtherPathMesh"), CubeMesh);
	OpponentPathMesh = CreateOverlayMesh(TEXT("OpponentPathMesh"), CubeMesh);
	DangerMarkerMesh = CreateOverlayMesh(TEXT("DangerMarkerMesh"), SphereMesh);
	DeliverMarkerMesh = CreateOverlayMesh(TEXT("DeliverMarkerMesh"), SphereMesh);
	ClaimMarkerMesh = CreateOverlayMesh(TEXT("ClaimMarkerMesh"), SphereMesh);
}

UInstancedStaticMeshComponent* AGatherAndHuntPuzzleBoardView::CreateOverlayMesh(const TCHAR* Name, UStaticMesh* Mesh)
{
	UInstancedStaticMeshComponent* const Component = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
	Component->SetupAttachment(Root);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	return Component;
}

void AGatherAndHuntPuzzleBoardView::TintOverlay(UInstancedStaticMeshComponent* Target, const FLinearColor& Color)
{
	if (TintableMaterial != nullptr)
	{
		UMaterialInstanceDynamic* const Tinted = UMaterialInstanceDynamic::Create(TintableMaterial, this);
		Tinted->SetVectorParameterValue(TEXT("Color"), Color);
		Target->SetMaterial(0, Tinted);
	}
}

void AGatherAndHuntPuzzleBoardView::Bind(UGatherAndHuntPuzzleSession* InSession, UGatherAndHuntTurnPlanner* InPlanner)
{
	if (Planner != nullptr)
	{
		Planner->OnPlanChanged.RemoveDynamic(this, &AGatherAndHuntPuzzleBoardView::HandlePlanChanged);
		Planner->OnPlanSubmitted.RemoveDynamic(this, &AGatherAndHuntPuzzleBoardView::HandlePlanSubmitted);
	}
	Playback->OnPlaybackFinished.RemoveDynamic(this, &AGatherAndHuntPuzzleBoardView::HandlePlaybackFinished);

	Session = InSession;
	Planner = InPlanner;

	if (Planner != nullptr)
	{
		Planner->OnPlanChanged.AddDynamic(this, &AGatherAndHuntPuzzleBoardView::HandlePlanChanged);
		Planner->OnPlanSubmitted.AddDynamic(this, &AGatherAndHuntPuzzleBoardView::HandlePlanSubmitted);
	}
	Playback->OnPlaybackFinished.AddDynamic(this, &AGatherAndHuntPuzzleBoardView::HandlePlaybackFinished);

	TintOverlay(BoundsMesh, FLinearColor(0.25f, 0.25f, 0.3f));
	TintOverlay(SelectedPathMesh, FLinearColor(0.0f, 0.8f, 1.0f));
	TintOverlay(OtherPathMesh, FLinearColor(0.2f, 0.35f, 1.0f));
	TintOverlay(OpponentPathMesh, OpponentTurtleColor);
	TintOverlay(DangerMarkerMesh, FLinearColor(1.0f, 0.0f, 0.0f));
	TintOverlay(DeliverMarkerMesh, FLinearColor(0.1f, 1.0f, 0.2f));
	TintOverlay(ClaimMarkerMesh, ObjectiveColor);

	EnsureLighting();
	RebuildBoard();
}

void AGatherAndHuntPuzzleBoardView::EnsureLighting()
{
	UWorld* const World = GetWorld();
	if (!bAddLightingIfMissing || World == nullptr || TActorIterator<ADirectionalLight>(World))
	{
		return;
	}
	if (FindComponentByClass<UDirectionalLightComponent>() != nullptr)
	{
		return; // Already added by an earlier Bind.
	}

	auto AddLight = [this](const FRotator& Rotation, const float Intensity, const int32 ForwardShadingPriority)
	{
		UDirectionalLightComponent* const Light = NewObject<UDirectionalLightComponent>(this);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetupAttachment(Root);
		Light->SetUsingAbsoluteRotation(true);
		Light->SetWorldRotation(Rotation);
		Light->SetIntensity(Intensity);
		Light->SetCastShadows(false);
		Light->ForwardShadingPriority = ForwardShadingPriority;
		Light->RegisterComponent();
		AddInstanceComponent(Light);
	};
	AddLight(FRotator(-50.0f, -35.0f, 0.0f), 6.0f, 1);
	AddLight(FRotator(-30.0f, 145.0f, 0.0f), 2.5f, 0);
}

void AGatherAndHuntPuzzleBoardView::ClearPieces()
{
	Playback->StopPlayback(false);
	Playback->ClearRegisteredActors();
	for (AGatherAndHuntPuzzlePieceActor* const Piece : Pieces)
	{
		if (IsValid(Piece))
		{
			Piece->Destroy();
		}
	}
	Pieces.Reset();
}

void AGatherAndHuntPuzzleBoardView::RebuildBoard()
{
	ClearPieces();
	BoundsMesh->ClearInstances();

	if (Session == nullptr || !Session->IsPuzzleLoaded())
	{
		RedrawPreview();
		return;
	}

	CellSize = Session->GetGridCellSize();
	Playback->GridCellSize = CellSize;

	UWorld* const World = GetWorld();
	const FGatherAndHuntPuzzleBoardSnapshot Snapshot = Session->GetCurrentBoardSnapshot();
	const float PieceScale = CellSize / BasicShapeSize;

	auto SpawnPiece = [&](UStaticMesh* Mesh, const FLinearColor& Color, const FRotator& MeshRotation, float Scale, const FString& Label)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGatherAndHuntPuzzlePieceActor* const Piece = World->SpawnActor<AGatherAndHuntPuzzlePieceActor>(Params);
		Piece->Setup(Mesh, TintableMaterial, Color, MeshRotation, Scale, Label);
		Pieces.Add(Piece);
		return Piece;
	};

	for (const FGatherAndHuntPuzzleTurtleState& Turtle : Snapshot.Turtles)
	{
		// Cones point along +Z; pitch -90 turns the tip to the actor's forward (+X).
		AGatherAndHuntPuzzlePieceActor* const Piece = SpawnPiece(
			ConeMesh,
			Turtle.OwnerId == 0 ? PlayerTurtleColor : OpponentTurtleColor,
			FRotator(-90.0f, 0.0f, 0.0f),
			PieceScale * 0.6f,
			FString::Printf(TEXT("T%d"), Turtle.TurtleId));
		Playback->RegisterTurtleActor(Turtle.TurtleId, Piece);
	}

	for (const FGatherAndHuntPuzzleObstacleState& Obstacle : Snapshot.Obstacles)
	{
		const bool bHunter = Obstacle.ObstacleKind == ObstacleKindHunter;
		const bool bHazard = Obstacle.ObstacleKind == ObstacleKindWandering;
		AGatherAndHuntPuzzlePieceActor* const Piece = SpawnPiece(
			bHunter ? SphereMesh : CubeMesh,
			bHunter ? HunterColor : (bHazard ? HazardColor : StaticObstacleColor),
			FRotator::ZeroRotator,
			PieceScale * (bHunter ? 0.7f : 0.85f),
			bHunter ? FString::Printf(TEXT("H%d"), Obstacle.ObstacleId)
				: (bHazard ? FString::Printf(TEXT("Z%d"), Obstacle.ObstacleId) : FString()));
		Playback->RegisterObstacleActor(Obstacle.ObstacleId, Piece);
	}

	for (const FGatherAndHuntPuzzleObjectiveState& Objective : Snapshot.Objectives)
	{
		const float ValueScale = FMath::Lerp(0.3f, 0.6f, FMath::Clamp(Objective.PointValue / 10.0f, 0.0f, 1.0f));
		AGatherAndHuntPuzzlePieceActor* const Piece = SpawnPiece(
			SphereMesh,
			ObjectiveColor,
			FRotator::ZeroRotator,
			PieceScale * ValueScale,
			FString::Printf(TEXT("+%d"), Objective.PointValue));
		Playback->RegisterObjectiveActor(Objective.ObjectiveId, Piece);
	}

	Playback->ApplyBoardSnapshotInstant(Snapshot);
	DrawBounds();
	FrameBoard();
	RedrawPreview();
}

void AGatherAndHuntPuzzleBoardView::DrawBounds()
{
	if (Session == nullptr)
	{
		return;
	}
	const FIntVector Size = Session->GetCurrentBoardSnapshot().GridSize;
	const float Half = CellSize * 0.5f;
	const FVector Min = GridToWorld(FIntVector::ZeroValue) - FVector(Half);
	const FVector Max = GridToWorld(Size - FIntVector(1)) + FVector(Half);
	const float EdgeThickness = CellSize * 0.04f;
	const float GridThickness = CellSize * 0.015f;

	// Box edges.
	const FVector Corners[8] = {
		FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z),
		FVector(Min.X, Min.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z), FVector(Min.X, Max.Y, Max.Z)};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		AddBar(BoundsMesh, Corners[Index], Corners[(Index + 1) % 4], EdgeThickness);
		AddBar(BoundsMesh, Corners[Index + 4], Corners[(Index + 1) % 4 + 4], EdgeThickness);
		AddBar(BoundsMesh, Corners[Index], Corners[Index + 4], EdgeThickness);
	}

	// Cell lines on the floor so positions are readable.
	for (int32 X = 1; X < Size.X; ++X)
	{
		const float LineX = Min.X + X * CellSize;
		AddBar(BoundsMesh, FVector(LineX, Min.Y, Min.Z), FVector(LineX, Max.Y, Min.Z), GridThickness);
	}
	for (int32 Y = 1; Y < Size.Y; ++Y)
	{
		const float LineY = Min.Y + Y * CellSize;
		AddBar(BoundsMesh, FVector(Min.X, LineY, Min.Z), FVector(Max.X, LineY, Min.Z), GridThickness);
	}
}

void AGatherAndHuntPuzzleBoardView::RedrawPreview()
{
	SelectedPathMesh->ClearInstances();
	OtherPathMesh->ClearInstances();
	OpponentPathMesh->ClearInstances();
	DangerMarkerMesh->ClearInstances();
	DeliverMarkerMesh->ClearInstances();
	ClaimMarkerMesh->ClearInstances();

	if (Planner == nullptr || !Planner->HasBoard() || IsPlayingBack())
	{
		return;
	}

	// Cyan/blue for the side being edited; magenta for the other side, which is
	// the scripted opponent normally and the player's queued plan when authoring.
	const bool bEditingOpponent = Planner->IsEditingOpponent();
	const int32 EditedOwner = bEditingOpponent ? 1 - Planner->PlayerId : Planner->PlayerId;
	for (const FGatherAndHuntPlannerTurtleTrack& Track : Planner->GetPreviewTracks())
	{
		if (Track.OwnerId != EditedOwner)
		{
			if (bEditingOpponent || (bShowOpponentPreview && Planner->GetPreviewIncludesOpponent()))
			{
				DrawTrack(OpponentPathMesh, Track);
			}
			continue;
		}
		DrawTrack(Track.TurtleId == Planner->GetSelectedTurtleId() ? SelectedPathMesh.Get() : OtherPathMesh.Get(), Track);
	}
}

void AGatherAndHuntPuzzleBoardView::DrawTrack(UInstancedStaticMeshComponent* PathMesh, const FGatherAndHuntPlannerTurtleTrack& Track)
{
	const bool bSelected = PathMesh == SelectedPathMesh;
	if (Track.Steps.Num() <= 1 && !bSelected)
	{
		return;
	}

	const float BarThickness = CellSize * (bSelected ? 0.1f : 0.06f);
	for (int32 Index = 1; Index < Track.Steps.Num(); ++Index)
	{
		const FGatherAndHuntPlannerTrackStep& Prev = Track.Steps[Index - 1];
		const FGatherAndHuntPlannerTrackStep& Step = Track.Steps[Index];
		const FVector StepLocation = GridToWorld(Step.GridPosition);

		if (Step.GridPosition != Prev.GridPosition)
		{
			AddBar(PathMesh, GridToWorld(Prev.GridPosition), StepLocation, BarThickness);
			AddCube(PathMesh, StepLocation, BarThickness * 1.8f);
		}

		switch (Step.Kind)
		{
		case EGatherAndHuntPlannerStepKind::Destroyed:
		case EGatherAndHuntPlannerStepKind::Blocked:
			AddCube(DangerMarkerMesh, StepLocation, CellSize * 0.45f);
			break;
		case EGatherAndHuntPlannerStepKind::Delivered:
			AddCube(DeliverMarkerMesh, StepLocation, CellSize * 0.4f);
			break;
		default:
			break;
		}

		if (Step.bClaimedObjective)
		{
			AddCube(ClaimMarkerMesh, StepLocation + FVector(0.0f, 0.0f, CellSize * 0.35f), CellSize * 0.2f);
		}
	}

	// Nose on the final step shows which way the turtle ends up facing.
	const FGatherAndHuntPlannerTrackStep& Last = Track.Steps.Last();
	if (Last.Kind != EGatherAndHuntPlannerStepKind::Destroyed && Last.Kind != EGatherAndHuntPlannerStepKind::Delivered)
	{
		const FVector End = GridToWorld(Last.GridPosition);
		AddBar(PathMesh, End, End + ForwardFromHeadingPitch(Last.Heading, Last.Pitch) * CellSize * 0.45f, BarThickness * 0.7f);
	}
}

void AGatherAndHuntPuzzleBoardView::AddBar(UInstancedStaticMeshComponent* Target, const FVector& From, const FVector& To, const float Thickness)
{
	const FVector Delta = To - From;
	const float Length = Delta.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FTransform Transform(
		Delta.Rotation(),
		(From + To) * 0.5f,
		FVector(Length, Thickness, Thickness) / BasicShapeSize);
	Target->AddInstance(Transform, true);
}

void AGatherAndHuntPuzzleBoardView::AddCube(UInstancedStaticMeshComponent* Target, const FVector& Center, const float Size)
{
	Target->AddInstance(FTransform(FRotator::ZeroRotator, Center, FVector(Size / BasicShapeSize)), true);
}

FVector AGatherAndHuntPuzzleBoardView::GridToWorld(const FIntVector& GridPosition) const
{
	// Must match UGatherAndHuntPuzzlePlaybackComponent::GridToWorld.
	return FVector(GridPosition) * CellSize;
}

void AGatherAndHuntPuzzleBoardView::FrameBoard()
{
	if (Session == nullptr || !Session->IsPuzzleLoaded())
	{
		return;
	}
	const FIntVector Size = Session->GetCurrentBoardSnapshot().GridSize;
	const FVector Center = (GridToWorld(FIntVector::ZeroValue) + GridToWorld(Size - FIntVector(1))) * 0.5f;
	CameraArm->SetWorldLocation(Center);
	CameraArm->TargetArmLength = FVector(Size).Size() * CellSize * 1.4f;
}

void AGatherAndHuntPuzzleBoardView::OrbitCamera(const float DeltaYaw, const float DeltaPitch)
{
	FRotator Rotation = CameraArm->GetRelativeRotation();
	Rotation.Yaw += DeltaYaw;
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + DeltaPitch, -89.0f, 89.0f);
	CameraArm->SetRelativeRotation(Rotation);
}

void AGatherAndHuntPuzzleBoardView::ZoomCamera(const float Steps)
{
	CameraArm->TargetArmLength = FMath::Clamp(CameraArm->TargetArmLength * FMath::Pow(0.9f, Steps), CellSize * 2.0f, CellSize * 200.0f);
}

bool AGatherAndHuntPuzzleBoardView::IsPlayingBack() const
{
	return Playback != nullptr && Playback->IsPlaying();
}

void AGatherAndHuntPuzzleBoardView::SkipPlayback()
{
	// Fires OnPlaybackFinished (-> HandlePlaybackFinished) if it was playing.
	Playback->StopPlayback(true);
}

void AGatherAndHuntPuzzleBoardView::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FVector ViewLocation = Camera->GetComponentLocation();
	for (AGatherAndHuntPuzzlePieceActor* const Piece : Pieces)
	{
		if (IsValid(Piece))
		{
			Piece->FaceLabelTowards(ViewLocation);
		}
	}
}

void AGatherAndHuntPuzzleBoardView::HandlePlanChanged()
{
	RedrawPreview();
}

void AGatherAndHuntPuzzleBoardView::HandlePlanSubmitted()
{
	// An empty turn finishes synchronously inside StartPlayback. Otherwise the
	// ghost paths stay hidden until OnPlaybackFinished.
	Playback->StartPlaybackFromSession(Session);
	RedrawPreview();
}

void AGatherAndHuntPuzzleBoardView::HandlePlaybackFinished()
{
	RedrawPreview();
	OnPlaybackFinished.Broadcast();
}
