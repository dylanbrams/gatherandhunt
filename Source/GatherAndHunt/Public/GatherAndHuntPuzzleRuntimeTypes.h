#pragma once

#include "CoreMinimal.h"
#include "GatherAndHuntPuzzleRuntimeTypes.generated.h"

UENUM(BlueprintType)
enum class EGatherAndHuntPuzzleMoveKind : uint8
{
	Forward,
	YawLeft,
	YawRight,
	PitchUp,
	PitchDown,
	Wait
};

UENUM(BlueprintType)
enum class EGatherAndHuntPuzzleOutcome : uint8
{
	NotLoaded,
	InProgress,
	Victory,
	Defeat
};

UENUM(BlueprintType)
enum class EGatherAndHuntPuzzleEventType : uint8
{
	TurtleTurned,
	TurtleWaited,
	TurtleBlocked,
	TurtleMoved,
	TurtleDestroyed,
	TurtleDelivered,
	TurtleClaimedObjective,
	ObstacleMoved,
	ObstacleDestroyed,
	HunterRetargeted
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleTurtlePlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TurtleId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EGatherAndHuntPuzzleMoveKind> Moves;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleTurnPlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FGatherAndHuntPuzzleTurtlePlan> TurtlePlans;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzlePlanValidation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsValid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ErrorMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OffendingTurtleId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OffendingMoveIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleTurtleState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TurtleId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OwnerId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector GridPosition = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Heading = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Pitch = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAlive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDelivered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CarriedPoints = 0;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleObstacleState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ObstacleId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ObstacleKind = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PatternKind = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector GridPosition = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAlive = true;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ObjectiveId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector GridPosition = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PointValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bClaimed = false;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleBoardSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector GridSize = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TurnNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ScorePlayer0 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ScorePlayer1 = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FGatherAndHuntPuzzleTurtleState> Turtles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FGatherAndHuntPuzzleObstacleState> Obstacles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FGatherAndHuntPuzzleObjectiveState> Objectives;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleTickEventView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGatherAndHuntPuzzleEventType Type = EGatherAndHuntPuzzleEventType::TurtleWaited;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PrimaryId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SecondaryId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector FromPosition = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector ToPosition = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FromHeading = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FromPitch = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ToHeading = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ToPitch = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PointsDelta = 0;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleTickResultView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TickIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FGatherAndHuntPuzzleTickEventView> Events;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleTurnResultView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FGatherAndHuntPuzzleTickResultView> Ticks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGatherAndHuntPuzzleBoardSnapshot FinalBoard;
};
