#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "UObject/Object.h"
#include "GatherAndHuntCampaign.generated.h"

class ULevelSave;

// What finishing a level adds to the ship's allowance on the level select map.
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntCampaignReward
{
	GENERATED_BODY()

	// Extra moves per sortie.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	int32 Moves = 0;

	// Motions (move buttons) unlocked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	TArray<EGatherAndHuntPuzzleMoveKind> Motions;
};

// A level marker on the map.
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntCampaignNode
{
	GENERATED_BODY()

	// Stable key for the save file and the travel URL: letters, digits, _ and -.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	FString Title;

	// Grid cell on the map board.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	FIntVector Cell = FIntVector::ZeroValue;

	// Board file under Content/BoardSetups that the marker opens.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	FString Puzzle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	FGatherAndHuntCampaignReward Reward;
};

// The level select map as authored (Content/BoardSetups/Campaign/Campaign.json).
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntCampaignDefinition
{
	GENERATED_BODY()

	// Board file for the map itself: its size, blocks and hazards, and the ship
	// at its starting cell. Edit it in the board editor like any puzzle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	FString MapBoard;

	// The allowance before any level is finished.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	int32 StartingMoves = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	TArray<EGatherAndHuntPuzzleMoveKind> StartingMotions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GatherAndHunt|Campaign")
	TArray<FGatherAndHuntCampaignNode> Nodes;
};

// What the ship can do on the map: the starting allowance plus the rewards of
// every finished level.
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntCampaignAllowance
{
	GENERATED_BODY()

	// Moves per sortie.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Campaign")
	int32 Moves = 0;

	// Bit (1 << EGatherAndHuntPuzzleMoveKind) per motion, as ULevelSave::AllowedMoveMask.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|Campaign")
	int32 MoveMask = 0;
};

// The player's progress. Only the finished levels are stored; the allowance is
// worked out from the definition, so retuning rewards applies to old saves.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntCampaignSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FString> CompletedNodeIds;
};

// The level select map: a board the ship flies over with a limited set of
// motions and a limited number of moves. Level markers sit on its cells; a
// sortie that ends having touched one opens that level, and winning the level
// adds to the allowance, which brings further markers into range.
//
// The map is played with the ordinary puzzle session, planner and board view:
// BuildMapLevelSave returns the map board with a ball per marker and the
// allowance's motions as its allowed moves.
UCLASS(BlueprintType)
class GATHERANDHUNT_API UGatherAndHuntCampaign : public UObject
{
	GENERATED_BODY()

public:
	// Under Content/BoardSetups.
	static const TCHAR* const DefinitionFile;
	static const TCHAR* const DefaultSaveSlot;

	// Reads the definition file and the progress in SaveSlot. Null, with the
	// reason, if the definition can't be read.
	static UGatherAndHuntCampaign* Load(UObject* Outer, FString& OutError, const FString& SaveSlot = FString(DefaultSaveSlot));

	// From a definition in memory. With an empty SaveSlot progress isn't stored.
	static UGatherAndHuntCampaign* Create(UObject* Outer, const FGatherAndHuntCampaignDefinition& InDefinition, const FString& SaveSlot = FString());

	static bool ParseDefinition(const FString& Json, FGatherAndHuntCampaignDefinition& OutDefinition, FString& OutError);

	const FGatherAndHuntCampaignDefinition& GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Campaign")
	const TArray<FGatherAndHuntCampaignNode>& GetNodes() const { return Definition.Nodes; }

	// INDEX_NONE if there is no such node.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Campaign")
	int32 FindNodeIndex(const FString& NodeId) const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Campaign")
	bool IsNodeCompleted(int32 NodeIndex) const;

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|Campaign")
	FGatherAndHuntCampaignAllowance GetAllowance() const;

	// Marks a level finished and stores the progress. False if it already was
	// (replaying a level earns nothing) or the id is unknown.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Campaign")
	bool CompleteNode(const FString& NodeId);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|Campaign")
	void ResetProgress();

	// The map as a puzzle: the map board plus a gold objective per marker, reach
	// rules, and the allowance's motions as
	// the allowed moves. The caller applies the allowance's move count with
	// UGatherAndHuntPuzzleSession::SetMaxMovesPerTurn. Null, with the reason, if
	// the map board can't be loaded.
	ULevelSave* BuildMapLevelSave(UObject* Outer, FString& OutError);

	// A label per marker on the last built map, by objective id: the level's
	// number in roman numerals, or its full name.
	TMap<int32, FString> GetMarkerLabels(bool bFullNames) const;

	// Objective ids of the finished levels' markers on the last built map.
	TSet<int32> GetCompletedMarkerIds() const;

	// The node's place in the list as a roman numeral, from I.
	static FString RomanNumeral(int32 NodeIndex);

	// The node's title, or its id if it has none.
	FString GetNodeName(int32 NodeIndex) const;

	// The level a resolved sortie enters: the last marker a ship touched, as long
	// as that ship survived the turn. INDEX_NONE if none. Uses the ids of the
	// last built map.
	int32 FindEnteredNode(const FGatherAndHuntPuzzleTurnResultView& TurnResult) const;

	// Nodes a sortie can enter with the current allowance, searched with the
	// sim's own resolver one move at a time for the map's first player ship.
	// Exact while the map holds only static blocks; moving hazards and chasers
	// can make a marker look reachable when the timing doesn't work out.
	TSet<int32> FindReachableNodes();

	// Problems an author should fix: duplicate or malformed ids, missing puzzle
	// files, markers off the board or sharing a cell. True if there are none.
	bool Validate(TArray<FString>& OutIssues);

	// "+2 moves, YAW L, YAW R"; empty for an empty reward.
	static FString DescribeReward(const FGatherAndHuntCampaignReward& Reward);

	// "FWD, YAW L"; "none" for an empty mask.
	static FString DescribeMotions(int32 MoveMask);

	static const TCHAR* MotionLabel(EGatherAndHuntPuzzleMoveKind Motion);

private:
	void LoadProgress();
	void SaveProgress() const;

	FGatherAndHuntCampaignDefinition Definition;
	FString SaveSlot;
	TSet<FString> CompletedNodeIds;

	// Objective piece id per node on the last built map, parallel to Definition.Nodes.
	TArray<int32> MarkerObjectiveIds;
};
