#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPuzzleBoardView.generated.h"

class AGatherAndHuntPuzzlePieceActor;
class UCameraComponent;
class UGatherAndHuntBoardEditor;
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

	// Respawns piece actors from the session's current board. Reframes the
	// camera if asked, or if the board's size changed.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void RebuildBoard(bool bReframe = true);

	// While set, draws the board editor's cursor and hazard paths instead of the
	// planner's preview, and follows its edits. nullptr goes back to the planner.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void SetBoardEditor(UGatherAndHuntBoardEditor* InBoardEditor);

	// Cell under a view ray: the nearest piece it passes through, else where it
	// crosses the board editor's cursor layer. False if it misses the board.
	bool PickEditorCell(const FVector& RayOrigin, const FVector& RayDirection, FIntVector& OutCell) const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardView")
	float GetCameraYaw() const;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void RedrawPreview();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void OrbitCamera(float DeltaYaw, float DeltaPitch);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void ZoomCamera(float Steps);

	// Turns the camera where it stands, as OrbitCamera turns it about the point
	// it is looking at.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void LookCamera(float DeltaYaw, float DeltaPitch);

	// Flies the camera: Axes is (forward along the view, right, world up), each
	// -1..1. Speed grows with the distance to the point it is looking at.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void FlyCamera(FVector Axes, float DeltaSeconds, bool bFast);

	// Slides the camera sideways and up/down by a mouse drag, in pixels.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|BoardView")
	void PanCamera(float DeltaRight, float DeltaUp);

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

	// Labels to show on objectives in place of their "+N", by objective id (the
	// level select map names its level markers). Takes effect on the next
	// RebuildBoard or RefreshObjectiveLabels.
	TMap<int32, FString> ObjectiveLabels;

	// Objectives drawn see-through with a dimmed label (the level select map's
	// finished levels). Takes effect on the next RebuildBoard.
	TSet<int32> FadedObjectiveIds;

	// Relabels the objectives already on the board from ObjectiveLabels.
	void RefreshObjectiveLabels();

	// Shared role palette for pieces, paths and the planner UI.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|BoardView")
	static FLinearColor GetRoleColor(EGatherAndHuntTurtleRole TurtleRole);

	// Adds key, fill and under lights when the level has no directional light, so the
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

	UFUNCTION()
	void HandleBoardEditorChanged(bool bBoardChanged);

	void DrawEditorOverlay();
	void ClearPieces();
	void EnsureLighting();
	void DrawBounds();
	void DrawGround(const FIntVector& Size);
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

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> EditorCursorMesh;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> EditorLayerMesh;

	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> HazardPathMesh;

	// Grass field under the board, for puzzles that ask for it.
	UPROPERTY(VisibleAnywhere, Category = "GatherAndHunt|BoardView")
	TObjectPtr<UInstancedStaticMeshComponent> GroundMesh;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntTurnPlanner> Planner;

	UPROPERTY(Transient)
	TObjectPtr<UGatherAndHuntBoardEditor> BoardEditor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGatherAndHuntPuzzlePieceActor>> Pieces;

	// The objectives among Pieces, by objective id.
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AGatherAndHuntPuzzlePieceActor>> ObjectivePieces;

	// Board size the camera was last framed for.
	FIntVector FramedGridSize = FIntVector::ZeroValue;

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

	// As TintableMaterial, but the colour's alpha is its opacity.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> TranslucentMaterial;

	float CellSize = 100.0f;
};
