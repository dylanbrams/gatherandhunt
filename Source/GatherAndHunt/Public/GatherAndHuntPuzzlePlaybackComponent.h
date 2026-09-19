#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "GatherAndHuntPuzzlePlaybackComponent.generated.h"

class UGatherAndHuntPuzzleSession;

UENUM(BlueprintType)
enum class EGatherAndHuntPuzzlePlaybackEntityKind : uint8
{
	Turtle,
	Obstacle,
	Objective
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGatherAndHuntPuzzlePlaybackSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGatherAndHuntPuzzlePlaybackTickEvent, int32, TickIndex);

UCLASS(ClassGroup=(GatherAndHunt), meta=(BlueprintSpawnableComponent))
class GATHERANDHUNT_API UGatherAndHuntPuzzlePlaybackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGatherAndHuntPuzzlePlaybackComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void RegisterActor(EGatherAndHuntPuzzlePlaybackEntityKind EntityKind, int32 EntityId, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void RegisterTurtleActor(int32 TurtleId, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void RegisterObstacleActor(int32 ObstacleId, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void RegisterObjectiveActor(int32 ObjectiveId, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void ClearRegisteredActors();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void ApplyBoardSnapshotInstant(const FGatherAndHuntPuzzleBoardSnapshot& BoardSnapshot);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	bool StartPlayback(
		const FGatherAndHuntPuzzleBoardSnapshot& InitialBoardSnapshot,
		const FGatherAndHuntPuzzleTurnResultView& TurnResultView);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	bool StartPlaybackFromSession(UGatherAndHuntPuzzleSession* PuzzleSession);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void StopPlayback(bool bApplyFinalBoard = true);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void PausePlayback();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void ResumePlayback();

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Puzzle|Playback")
	void StepToNextTick();

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle|Playback")
	bool IsPlaying() const { return bIsPlaying; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle|Playback")
	bool IsPaused() const { return bIsPaused; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Puzzle|Playback")
	int32 GetCurrentTickIndex() const { return ActiveTickIndex; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Puzzle|Playback")
	float SecondsPerTick = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Puzzle|Playback")
	float GridCellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Puzzle|Playback")
	bool bAutoHideDestroyedActors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Puzzle|Playback")
	bool bAutoHideDeliveredTurtles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Puzzle|Playback")
	bool bAutoHideClaimedObjectives = true;

	UPROPERTY(BlueprintAssignable, Category = "GatherAndHunt|Puzzle|Playback")
	FGatherAndHuntPuzzlePlaybackSimpleEvent OnPlaybackStarted;

	UPROPERTY(BlueprintAssignable, Category = "GatherAndHunt|Puzzle|Playback")
	FGatherAndHuntPuzzlePlaybackTickEvent OnPlaybackTickStarted;

	UPROPERTY(BlueprintAssignable, Category = "GatherAndHunt|Puzzle|Playback")
	FGatherAndHuntPuzzlePlaybackTickEvent OnPlaybackTickFinished;

	UPROPERTY(BlueprintAssignable, Category = "GatherAndHunt|Puzzle|Playback")
	FGatherAndHuntPuzzlePlaybackSimpleEvent OnPlaybackFinished;

private:
	struct FPlaybackBinding
	{
		TWeakObjectPtr<AActor> Actor;
		FVector StartLocation = FVector::ZeroVector;
		FVector TargetLocation = FVector::ZeroVector;
		FRotator StartRotation = FRotator::ZeroRotator;
		FRotator TargetRotation = FRotator::ZeroRotator;
		bool bPendingHideAtTickEnd = false;
	};

	void BeginTickPlayback(int32 TickIndex);
	void FinishTickPlayback();
	void ApplyFinalBoardSnapshot();
	void SetActorTransform(const TWeakObjectPtr<AActor>& Actor, const FVector& Location, const FRotator& Rotation, bool bVisible) const;
	void UpdateBindingTarget(FPlaybackBinding& Binding, const FVector& TargetLocation, const FRotator& TargetRotation);
	FVector GridToWorld(const FIntVector& GridPosition) const;
	FRotator RotationFromHeadingPitch(int32 Heading, int32 Pitch) const;
	FPlaybackBinding* FindBinding(EGatherAndHuntPuzzlePlaybackEntityKind EntityKind, int32 EntityId);
	const FPlaybackBinding* FindBinding(EGatherAndHuntPuzzlePlaybackEntityKind EntityKind, int32 EntityId) const;
	void UpdateActorsForInterpolation(float Alpha);

	TMap<int32, FPlaybackBinding> TurtleBindings;
	TMap<int32, FPlaybackBinding> ObstacleBindings;
	TMap<int32, FPlaybackBinding> ObjectiveBindings;

	FGatherAndHuntPuzzleBoardSnapshot ActiveInitialBoardSnapshot;
	FGatherAndHuntPuzzleTurnResultView ActiveTurnResultView;
	bool bIsPlaying = false;
	bool bIsPaused = false;
	int32 ActiveTickIndex = INDEX_NONE;
	float TickElapsedSeconds = 0.0f;
};
