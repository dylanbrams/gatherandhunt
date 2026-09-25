#include "GatherAndHuntPuzzleBoardView.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "GatherAndHuntBoardEditor.h"
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

	const FLinearColor StaticObstacleColor(0.35f, 0.35f, 0.38f);
	const FLinearColor HazardColor(1.0f, 0.45f, 0.0f);
	const FLinearColor HunterColor(0.9f, 0.05f, 0.05f);
	const FLinearColor ObjectiveColor(1.0f, 0.8f, 0.0f);
	const FLinearColor GrassColor(0.12f, 0.38f, 0.08f);

	// How far the grass field reaches past the board, in cells.
	constexpr float GroundMarginCells = 30.0f;

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
	PlayerPathMesh = CreateOverlayMesh(TEXT("PlayerPathMesh"), CubeMesh);
	AllyPathMesh = CreateOverlayMesh(TEXT("AllyPathMesh"), CubeMesh);
	OpponentPathMesh = CreateOverlayMesh(TEXT("OpponentPathMesh"), CubeMesh);
	DangerMarkerMesh = CreateOverlayMesh(TEXT("DangerMarkerMesh"), SphereMesh);
	DeliverMarkerMesh = CreateOverlayMesh(TEXT("DeliverMarkerMesh"), SphereMesh);
	ClaimMarkerMesh = CreateOverlayMesh(TEXT("ClaimMarkerMesh"), SphereMesh);
	EditorCursorMesh = CreateOverlayMesh(TEXT("EditorCursorMesh"), CubeMesh);
	EditorLayerMesh = CreateOverlayMesh(TEXT("EditorLayerMesh"), CubeMesh);
	HazardPathMesh = CreateOverlayMesh(TEXT("HazardPathMesh"), CubeMesh);
	GroundMesh = CreateOverlayMesh(TEXT("GroundMesh"), CubeMesh);
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
	TintOverlay(PlayerPathMesh, GetRoleColor(EGatherAndHuntTurtleRole::Player));
	TintOverlay(AllyPathMesh, GetRoleColor(EGatherAndHuntTurtleRole::Ally));
	TintOverlay(OpponentPathMesh, GetRoleColor(EGatherAndHuntTurtleRole::Opponent));
	TintOverlay(DangerMarkerMesh, FLinearColor(1.0f, 0.0f, 0.0f));
	TintOverlay(DeliverMarkerMesh, FLinearColor(0.1f, 1.0f, 0.2f));
	TintOverlay(ClaimMarkerMesh, ObjectiveColor);
	TintOverlay(EditorCursorMesh, FLinearColor(1.0f, 1.0f, 0.6f));
	TintOverlay(EditorLayerMesh, FLinearColor(0.45f, 0.45f, 0.2f));
	TintOverlay(HazardPathMesh, HazardColor);
	TintOverlay(GroundMesh, GrassColor);

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

void AGatherAndHuntPuzzleBoardView::RebuildBoard(const bool bReframe)
{
	ClearPieces();
	BoundsMesh->ClearInstances();
	GroundMesh->ClearInstances();

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
			GetRoleColor(Turtle.Role),
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
	if (Session->ShowsGround())
	{
		DrawGround(Snapshot.GridSize);
	}
	if (bReframe || Snapshot.GridSize != FramedGridSize)
	{
		FrameBoard();
	}
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

void AGatherAndHuntPuzzleBoardView::DrawGround(const FIntVector& Size)
{
	// A flat slab just under the bottom layer, reaching well past the board.
	const float Half = CellSize * 0.5f;
	const FVector Min = GridToWorld(FIntVector::ZeroValue) - FVector(Half);
	const FVector Max = GridToWorld(Size - FIntVector(1)) + FVector(Half);
	const float Thickness = CellSize * 0.1f;
	const FVector Center((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, Min.Z - Thickness * 0.5f - CellSize * 0.02f);
	const FVector Extent = (FVector(Max.X - Min.X, Max.Y - Min.Y, 0.0f) + FVector(2.0f * GroundMarginCells * CellSize));
	GroundMesh->AddInstance(FTransform(FRotator::ZeroRotator, Center, FVector(Extent.X, Extent.Y, Thickness) / BasicShapeSize), true);
}

void AGatherAndHuntPuzzleBoardView::RedrawPreview()
{
	SelectedPathMesh->ClearInstances();
	PlayerPathMesh->ClearInstances();
	AllyPathMesh->ClearInstances();
	OpponentPathMesh->ClearInstances();
	DangerMarkerMesh->ClearInstances();
	DeliverMarkerMesh->ClearInstances();
	ClaimMarkerMesh->ClearInstances();
	EditorCursorMesh->ClearInstances();
	EditorLayerMesh->ClearInstances();
	HazardPathMesh->ClearInstances();

	if (BoardEditor != nullptr)
	{
		DrawEditorOverlay();
		return;
	}
	if (Planner == nullptr || !Planner->HasBoard() || IsPlayingBack())
	{
		return;
	}

	// Selected turtle in cyan; everything else in its role colour. Scripted
	// paths show while authoring the script or when the preview includes it.
	const bool bShowScripted = Planner->IsEditingScript() || (bShowScriptedPreview && Planner->GetPreviewIncludesScripted());
	for (const FGatherAndHuntPlannerTurtleTrack& Track : Planner->GetPreviewTracks())
	{
		if (Track.TurtleId == Planner->GetSelectedTurtleId())
		{
			DrawTrack(SelectedPathMesh, Track);
			continue;
		}
		switch (Track.Role)
		{
		case EGatherAndHuntTurtleRole::Player: DrawTrack(PlayerPathMesh, Track); break;
		case EGatherAndHuntTurtleRole::Ally: if (bShowScripted) { DrawTrack(AllyPathMesh, Track); } break;
		case EGatherAndHuntTurtleRole::Opponent: if (bShowScripted) { DrawTrack(OpponentPathMesh, Track); } break;
		}
	}
}

FLinearColor AGatherAndHuntPuzzleBoardView::GetRoleColor(const EGatherAndHuntTurtleRole TurtleRole)
{
	switch (TurtleRole)
	{
	case EGatherAndHuntTurtleRole::Ally: return FLinearColor(0.15f, 0.45f, 1.0f);
	case EGatherAndHuntTurtleRole::Opponent: return FLinearColor(0.85f, 0.1f, 0.6f);
	default: return FLinearColor(0.15f, 0.8f, 0.3f);
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
	FramedGridSize = Size;
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

// --- Board editor -------------------------------------------------------------

void AGatherAndHuntPuzzleBoardView::SetBoardEditor(UGatherAndHuntBoardEditor* InBoardEditor)
{
	if (BoardEditor != nullptr)
	{
		BoardEditor->OnChanged.RemoveDynamic(this, &AGatherAndHuntPuzzleBoardView::HandleBoardEditorChanged);
	}
	BoardEditor = InBoardEditor;
	if (BoardEditor != nullptr)
	{
		BoardEditor->OnChanged.AddDynamic(this, &AGatherAndHuntPuzzleBoardView::HandleBoardEditorChanged);
	}
	RebuildBoard(false);
}

void AGatherAndHuntPuzzleBoardView::HandleBoardEditorChanged(const bool bBoardChanged)
{
	if (bBoardChanged)
	{
		RebuildBoard(false); // Redraws the overlay too.
	}
	else
	{
		RedrawPreview();
	}
}

void AGatherAndHuntPuzzleBoardView::DrawEditorOverlay()
{
	if (!BoardEditor->HasPuzzle())
	{
		return;
	}
	const FIntVector Count = BoardEditor->GetCellCount();
	const FIntVector Cursor = BoardEditor->GetCursor();
	const float Half = CellSize * 0.5f;

	// Wire cube around the cursor cell.
	{
		const FVector Center = GridToWorld(Cursor);
		const float Extent = Half * 0.98f;
		FVector Corners[8];
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Corners[Index] = Center + FVector((Index & 1) ? Extent : -Extent, (Index & 2) ? Extent : -Extent, (Index & 4) ? Extent : -Extent);
		}
		const int32 Edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
		for (const auto& Edge : Edges)
		{
			AddBar(EditorCursorMesh, Corners[Edge[0]], Corners[Edge[1]], CellSize * 0.05f);
		}
	}

	// Outline of the layer the cursor is on, and a drop line to the floor, so
	// the cursor's height reads from any angle.
	{
		const FVector Min = GridToWorld(FIntVector(0, 0, Cursor.Z)) - FVector(Half, Half, 0.0f);
		const FVector Max = GridToWorld(FIntVector(Count.X - 1, Count.Y - 1, Cursor.Z)) + FVector(Half, Half, 0.0f);
		const float Thickness = CellSize * 0.03f;
		AddBar(EditorLayerMesh, FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), Thickness);
		AddBar(EditorLayerMesh, FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Max.Y, Min.Z), Thickness);
		AddBar(EditorLayerMesh, FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z), Thickness);
		AddBar(EditorLayerMesh, FVector(Min.X, Max.Y, Min.Z), FVector(Min.X, Min.Y, Min.Z), Thickness);
		if (Cursor.Z > 0)
		{
			const FVector Floor = GridToWorld(FIntVector(Cursor.X, Cursor.Y, 0)) - FVector(0.0f, 0.0f, Half);
			AddBar(EditorLayerMesh, Floor, GridToWorld(Cursor) - FVector(0.0f, 0.0f, Half), Thickness);
		}
	}

	// Every moving hazard's cycle, thicker for the selected one.
	FGatherAndHuntBoardEditorPiece Selected;
	const int32 SelectedId = BoardEditor->GetSelectedPiece(Selected) ? Selected.PieceId : INDEX_NONE;
	for (const FGatherAndHuntBoardEditorPiece& Piece : BoardEditor->GetPieces())
	{
		if (!UGatherAndHuntBoardEditor::IsMovingHazard(Piece.Kind))
		{
			continue;
		}
		const TArray<FIntVector> Path = BoardEditor->GetHazardPath(Piece.PieceId);
		const float Thickness = CellSize * (Piece.PieceId == SelectedId ? 0.08f : 0.04f);
		for (int32 Index = 1; Index < Path.Num(); ++Index)
		{
			AddBar(HazardPathMesh, GridToWorld(Path[Index - 1]), GridToWorld(Path[Index]), Thickness);
			AddCube(HazardPathMesh, GridToWorld(Path[Index]), Thickness * 1.8f);
		}
	}
}

bool AGatherAndHuntPuzzleBoardView::PickEditorCell(const FVector& RayOrigin, const FVector& RayDirection, FIntVector& OutCell) const
{
	if (BoardEditor == nullptr || !BoardEditor->HasPuzzle() || RayDirection.IsNearlyZero())
	{
		return false;
	}
	const FVector Direction = RayDirection.GetSafeNormal();

	// Nearest piece the ray passes close to the centre of.
	float BestDistance = TNumericLimits<float>::Max();
	for (const FGatherAndHuntBoardEditorPiece& Piece : BoardEditor->GetPieces())
	{
		const FVector Center = GridToWorld(Piece.Cell);
		const float Along = FVector::DotProduct(Center - RayOrigin, Direction);
		if (Along > 0.0f && Along < BestDistance
			&& FVector::DistSquared(RayOrigin + Direction * Along, Center) <= FMath::Square(CellSize * 0.45f))
		{
			BestDistance = Along;
			OutCell = Piece.Cell;
		}
	}
	if (BestDistance < TNumericLimits<float>::Max())
	{
		return true;
	}

	// Otherwise the cell where the ray crosses the cursor's layer.
	const float LayerZ = GridToWorld(BoardEditor->GetCursor()).Z;
	if (FMath::IsNearlyZero(Direction.Z))
	{
		return false;
	}
	const float T = (LayerZ - RayOrigin.Z) / Direction.Z;
	if (T <= 0.0f)
	{
		return false;
	}
	const FVector Hit = RayOrigin + Direction * T;
	const FIntVector Cell(FMath::RoundToInt(Hit.X / CellSize), FMath::RoundToInt(Hit.Y / CellSize), BoardEditor->GetCursor().Z);
	const FIntVector Count = BoardEditor->GetCellCount();
	if (Cell.X < 0 || Cell.Y < 0 || Cell.X >= Count.X || Cell.Y >= Count.Y)
	{
		return false;
	}
	OutCell = Cell;
	return true;
}

float AGatherAndHuntPuzzleBoardView::GetCameraYaw() const
{
	return CameraArm->GetRelativeRotation().Yaw;
}
