#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPuzzleBoardView.generated.h"

class AGatherAndHuntPuzzlePieceActor;
class UCameraComponent;
class UGatherAndHuntPuzzlePlaybackComponent;
class UGatherAndHuntPuzzleSession;
class UGatherAndHuntTurnPlanner;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USpringArmComponent;
class UStaticMesh;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGatherAndHuntBoardViewEvent);

// Minimal scene for a puzzle session: placeholder piece actors, turn playback,
// the planner's ghost paths and an orbit camera around the board. Everything is
// built from engine basic shapes so it works in an empty map.
UCLASS()
class GATHERANDHUNT_API AGatherAndHuntPuzzleBoardView : public AActor
{
	GENERATED_BODY()

public:
	AGatherAndHuntPuzzleBoardView();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void Bind(UGatherAndHuntPuzzleSession* InSession, UGatherAndHuntTurnPlanner* InPlanner);

	// Respawns piece actors from the session's current board and reframes the camera.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void RebuildBoard();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void RedrawPreview();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void OrbitCamera(float DeltaYaw, float DeltaPitch);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void ZoomCamera(float Steps);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void FrameBoard();

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardView")
	bool IsPlayingBack() const;

	// Skips the rest of the current playback and snaps to the final board.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void SkipPlayback();

	UPROPERTY(BlueprintAssignable, Category = "GatherAndHunt|BoardView")
	FGatherAndHuntBoardViewEvent OnPlaybackFinished;

	// Draw scripted turtles' (allies and opponents) previewed paths when the
	// planner includes them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|BoardView")
	bool bShowScriptedPreview = true;

	// Shared role palette for pieces, paths and the planner UI.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardView")
	static FLinearColor GetRoleColor(EGatherAndHuntTurtleRole TurtleRole);

	// Adds a key and fill light when the level has no directional light, so the
	// board is readable in an empty map.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|BoardView")
	bool bAddLightingIfMissing = true;

private:
	UFUNCTION()
	void HandlePlanChanged();

	UFUNCTION()
	void HandlePlanSubmitted();

	UFUNCTION()
	void HandlePlaybackFinished();

	void ClearPieces();
	void EnsureLighting();
	void DrawBounds();
	void DrawTrack(UInstancedStaticMeshComponent* PathMesh, const struct FGatherAndHuntPlannerTurtleTrack& Track);
	void AddBar(UInstancedStaticMeshComponent* Target, const FVector& From, const FVector& To, float Thickness);
	void AddCube(UInstancedStaticMeshComponent* Target, const FVector& Center, float Size);
	FVector GridToWorld(const FIntVector& GridPosition) const;
	UInstancedStaticMeshComponent* CreateOverlayMesh(const TCHAR* Name, UStaticMesh* Mesh);
	void TintOverlay(UInstancedStaticMeshComponent* Target, const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UGatherAndHuntPuzzlePlaybackComponent> Playback;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> BoundsMesh;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> SelectedPathMesh;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> PlayerPathMesh;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> AllyPathMesh;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> OpponentPathMesh;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> DangerMarkerMesh;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> DeliverMarkerMesh;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> ClaimMarkerMesh;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntPuzzleSession> Session;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntTurnPlanner> Planner;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGatherAndHuntPuzzlePieceActor>> Pieces;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ConeMesh;

	// The basic shape meshes default to untintable materials in 5.8; this one
	// has a "Color" parameter.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> TintableMaterial;

	float CellSize = 100.0f;
};
