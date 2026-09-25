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

// Bit (1 << MoveKind) per move the player may use in a puzzle.
constexpr int32 GatherAndHuntAllMovesMask = 0x3F;

inline bool IsGatherAndHuntMoveAllowed(const int32 AllowedMoveMask, const EGatherAndHuntPuzzleMoveKind MoveKind)
{
	return (AllowedMoveMask & (1 << static_cast<int32>(MoveKind))) != 0;
}

// What wins a puzzle's objective targets.
UENUM(BlueprintType)
enum class EGatherAndHuntPuzzleGoal : uint8
{
	Deliver,  // Claim objectives and carry them off the board edge (the full rules).
	Reach     // Touching an objective is enough (the early tutorial puzzles).
};

// Who a turtle belongs to and who drives it. Team (sim Owner) decides scoring;
// control decides whether the player plans it or a puzzle script does.
UENUM(BlueprintType)
enum class EGatherAndHuntTurtleRole : uint8
{
	Player,   // Player's team, planned by the player.
	Ally,     // Player's team, moved by the puzzle script; scores for the player.
	Opponent  // Opponent team, moved by the puzzle script.
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
	EGatherAndHuntTurtleRole Role = EGatherAndHuntTurtleRole::Player;

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
