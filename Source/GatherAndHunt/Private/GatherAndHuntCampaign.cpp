#include "GatherAndHuntCampaign.h"

#include "GatherAndHuntFrontEnd.h"
#include "GatherAndHuntLevelEditorLibrary.h"
#include "GatherAndHuntPuzzleSession.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "ULevelSave.h"

#include "Resolver.h"

const TCHAR* const UGatherAndHuntCampaign::DefinitionFile = TEXT("Campaign/Campaign.json");
const TCHAR* const UGatherAndHuntCampaign::DefaultSaveSlot = TEXT("Campaign");

namespace
{
	const EGatherAndHuntPuzzleMoveKind AllMotions[] = {
		EGatherAndHuntPuzzleMoveKind::Forward, EGatherAndHuntPuzzleMoveKind::YawLeft, EGatherAndHuntPuzzleMoveKind::YawRight,
		EGatherAndHuntPuzzleMoveKind::PitchUp, EGatherAndHuntPuzzleMoveKind::PitchDown, EGatherAndHuntPuzzleMoveKind::Wait};

	int32 MotionMask(const TArray<EGatherAndHuntPuzzleMoveKind>& Motions)
	{
		int32 Mask = 0;
		for (const EGatherAndHuntPuzzleMoveKind Motion : Motions)
		{
			Mask |= 1 << static_cast<int32>(Motion);
		}
		return Mask;
	}

	bool IsObjectivePiece(const FPieceSaveData& Piece)
	{
		return Piece.PieceType == PieceTypeEnum::BP_Objective || Piece.PieceType == PieceTypeEnum::BP_GoldObjective;
	}

	bool IsValidNodeId(const FString& Id)
	{
		if (Id.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Id)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}
}

UGatherAndHuntCampaign* UGatherAndHuntCampaign::Load(UObject* Outer, FString& OutError, const FString& SaveSlot)
{
	const FString Path = UGatherAndHuntFrontEnd::GetBoardSetupsDir() / DefinitionFile;
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		OutError = FString::Printf(TEXT("Could not read %s."), *Path);
		return nullptr;
	}

	FGatherAndHuntCampaignDefinition Definition;
	if (!ParseDefinition(Json, Definition, OutError))
	{
		OutError = FString::Printf(TEXT("%s: %s"), DefinitionFile, *OutError);
		return nullptr;
	}
	return Create(Outer, Definition, SaveSlot);
}

UGatherAndHuntCampaign* UGatherAndHuntCampaign::Create(UObject* Outer, const FGatherAndHuntCampaignDefinition& InDefinition, const FString& SaveSlot)
{
	UGatherAndHuntCampaign* const Campaign = NewObject<UGatherAndHuntCampaign>(Outer != nullptr ? Outer : GetTransientPackage());
	Campaign->Definition = InDefinition;
	Campaign->SaveSlot = SaveSlot;
	Campaign->LoadProgress();
	return Campaign;
}

bool UGatherAndHuntCampaign::ParseDefinition(const FString& Json, FGatherAndHuntCampaignDefinition& OutDefinition, FString& OutError)
{
	FText FailReason;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutDefinition, 0, 0, false, &FailReason))
	{
		OutError = FailReason.IsEmpty() ? FString(TEXT("not valid JSON for a campaign definition.")) : FailReason.ToString();
		return false;
	}
	return true;
}

void UGatherAndHuntCampaign::LoadProgress()
{
	CompletedNodeIds.Reset();
	if (SaveSlot.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
	{
		return;
	}
	if (const UGatherAndHuntCampaignSave* const Save = Cast<UGatherAndHuntCampaignSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)))
	{
		CompletedNodeIds.Append(Save->CompletedNodeIds);
	}
}

void UGatherAndHuntCampaign::SaveProgress() const
{
	if (SaveSlot.IsEmpty())
	{
		return;
	}
	UGatherAndHuntCampaignSave* const Save = NewObject<UGatherAndHuntCampaignSave>();
	Save->CompletedNodeIds = CompletedNodeIds.Array();
	Save->CompletedNodeIds.Sort();
	UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
}

int32 UGatherAndHuntCampaign::FindNodeIndex(const FString& NodeId) const
{
	return Definition.Nodes.IndexOfByPredicate([&NodeId](const FGatherAndHuntCampaignNode& Node) { return Node.Id == NodeId; });
}

bool UGatherAndHuntCampaign::IsNodeCompleted(const int32 NodeIndex) const
{
	return Definition.Nodes.IsValidIndex(NodeIndex) && CompletedNodeIds.Contains(Definition.Nodes[NodeIndex].Id);
}

FGatherAndHuntCampaignAllowance UGatherAndHuntCampaign::GetAllowance() const
{
	FGatherAndHuntCampaignAllowance Allowance;
	Allowance.Moves = Definition.StartingMoves;
	Allowance.MoveMask = MotionMask(Definition.StartingMotions);
	for (int32 Index = 0; Index < Definition.Nodes.Num(); ++Index)
	{
		if (IsNodeCompleted(Index))
		{
			Allowance.Moves += Definition.Nodes[Index].Reward.Moves;
			Allowance.MoveMask |= MotionMask(Definition.Nodes[Index].Reward.Motions);
		}
	}
	Allowance.Moves = FMath::Max(0, Allowance.Moves);
	return Allowance;
}

bool UGatherAndHuntCampaign::CompleteNode(const FString& NodeId)
{
	if (FindNodeIndex(NodeId) == INDEX_NONE || CompletedNodeIds.Contains(NodeId))
	{
		return false;
	}
	CompletedNodeIds.Add(NodeId);
	SaveProgress();
	return true;
}

void UGatherAndHuntCampaign::ResetProgress()
{
	CompletedNodeIds.Reset();
	if (!SaveSlot.IsEmpty())
	{
		UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
	}
}

ULevelSave* UGatherAndHuntCampaign::BuildMapLevelSave(UObject* Outer, FString& OutError)
{
	MarkerObjectiveIds.Reset();
	ULevelSave* const Map = UGatherAndHuntLevelEditorLibrary::LoadLevelSaveFromProjectFile(Outer, Definition.MapBoard);
	if (Map == nullptr)
	{
		OutError = FString::Printf(TEXT("Could not load the map board \"%s\"."), *Definition.MapBoard);
		return nullptr;
	}

	// Markers come from the definition; balls placed on the map board itself
	// would be levels that lead nowhere.
	Map->PieceSaveDataArray.RemoveAll(&IsObjectivePiece);
	for (int32 Index = 0; Index < Definition.Nodes.Num(); ++Index)
	{
		FPieceSaveData Marker;
		Marker.PieceSaveID = Map->GetNextAvailablePieceSaveID();
		// Finished levels look the same here; the map screen fades them.
		Marker.PieceType = PieceTypeEnum::BP_GoldObjective;
		Marker.Position = FVector(Definition.Nodes[Index].Cell) * Map->GridCellSize;
		Map->PieceSaveDataArray.Add(Marker);
		MarkerObjectiveIds.Add(Marker.PieceSaveID);
	}

	// Touching a marker is what counts, and nothing ends the map.
	Map->Goal = EGatherAndHuntPuzzleGoal::Reach;
	Map->TargetObjectiveCount = 0;
	Map->TargetScore = 0;
	Map->TargetTurnCount = 0;
	Map->AllowedMoveMask = GetAllowance().MoveMask;
	return Map;
}

TMap<int32, FString> UGatherAndHuntCampaign::GetMarkerLabels(const bool bFullNames) const
{
	TMap<int32, FString> Labels;
	for (int32 Index = 0; Index < MarkerObjectiveIds.Num() && Index < Definition.Nodes.Num(); ++Index)
	{
		Labels.Add(MarkerObjectiveIds[Index], bFullNames ? GetNodeName(Index) : RomanNumeral(Index));
	}
	return Labels;
}

TSet<int32> UGatherAndHuntCampaign::GetCompletedMarkerIds() const
{
	TSet<int32> Ids;
	for (int32 Index = 0; Index < MarkerObjectiveIds.Num(); ++Index)
	{
		if (IsNodeCompleted(Index))
		{
			Ids.Add(MarkerObjectiveIds[Index]);
		}
	}
	return Ids;
}

FString UGatherAndHuntCampaign::GetNodeName(const int32 NodeIndex) const
{
	if (!Definition.Nodes.IsValidIndex(NodeIndex))
	{
		return FString();
	}
	const FGatherAndHuntCampaignNode& Node = Definition.Nodes[NodeIndex];
	return Node.Title.IsEmpty() ? Node.Id : Node.Title;
}

FString UGatherAndHuntCampaign::RomanNumeral(const int32 NodeIndex)
{
	static const int32 Values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
	static const TCHAR* const Symbols[] = {
		TEXT("M"), TEXT("CM"), TEXT("D"), TEXT("CD"), TEXT("C"), TEXT("XC"), TEXT("L"), TEXT("XL"), TEXT("X"), TEXT("IX"), TEXT("V"), TEXT("IV"), TEXT("I")};
	FString Numeral;
	int32 Remaining = NodeIndex + 1;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Values) && Remaining > 0; ++Index)
	{
		while (Remaining >= Values[Index])
		{
			Numeral += Symbols[Index];
			Remaining -= Values[Index];
		}
	}
	return Numeral;
}

int32 UGatherAndHuntCampaign::FindEnteredNode(const FGatherAndHuntPuzzleTurnResultView& TurnResult) const
{
	int32 Entered = INDEX_NONE;
	int32 EnteringTurtleId = INDEX_NONE;
	for (const FGatherAndHuntPuzzleTickResultView& Tick : TurnResult.Ticks)
	{
		for (const FGatherAndHuntPuzzleTickEventView& Event : Tick.Events)
		{
			const int32 Node = Event.Type == EGatherAndHuntPuzzleEventType::TurtleClaimedObjective
				? MarkerObjectiveIds.IndexOfByKey(Event.SecondaryId)
				: INDEX_NONE;
			if (Node != INDEX_NONE)
			{
				Entered = Node;
				EnteringTurtleId = Event.PrimaryId;
			}
		}
	}

	for (const FGatherAndHuntPuzzleTurtleState& Turtle : TurnResult.FinalBoard.Turtles)
	{
		if (Turtle.TurtleId == EnteringTurtleId)
		{
			return Turtle.bAlive && !Turtle.bDelivered ? Entered : INDEX_NONE;
		}
	}
	return INDEX_NONE;
}

TSet<int32> UGatherAndHuntCampaign::FindReachableNodes()
{
	TSet<int32> Reachable;
	FString Error;
	ULevelSave* const Map = BuildMapLevelSave(GetTransientPackage(), Error);
	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	if (Map == nullptr || !Session->LoadPuzzleFromLevelSave(Map))
	{
		return Reachable;
	}

	const turtlesim::BoardState& Start = *Session->GetCurrentBoardState();
	int32 ShipId = INDEX_NONE;
	for (const turtlesim::Turtle& Turtle : Start.Turtles)
	{
		if (Session->GetTurtleRole(Turtle.Id) == EGatherAndHuntTurtleRole::Player)
		{
			ShipId = Turtle.Id;
			break;
		}
	}
	if (ShipId == INDEX_NONE)
	{
		return Reachable;
	}

	// Breadth-first over the ship's pose, one single-move turn per step. The
	// marker last touched is part of the state, since it decides the level entered.
	struct FSearchState
	{
		turtlesim::BoardState Board;
		int32 LastNode = INDEX_NONE;
	};
	const FGatherAndHuntCampaignAllowance Allowance = GetAllowance();
	TSet<TTuple<FIntVector, int32, int32>> Seen;
	TArray<FSearchState> Frontier;
	Frontier.Add({Start, INDEX_NONE});
	const turtlesim::TurnPlan NoPlan;

	for (int32 Depth = 0; Depth < Allowance.Moves && Frontier.Num() > 0; ++Depth)
	{
		TArray<FSearchState> Next;
		for (const FSearchState& State : Frontier)
		{
			for (const EGatherAndHuntPuzzleMoveKind Motion : AllMotions)
			{
				if (!IsGatherAndHuntMoveAllowed(Allowance.MoveMask, Motion))
				{
					continue;
				}
				FGatherAndHuntPuzzleTurnPlan Plan;
				FGatherAndHuntPuzzleTurtlePlan& ShipPlan = Plan.TurtlePlans.AddDefaulted_GetRef();
				ShipPlan.TurtleId = ShipId;
				ShipPlan.Moves.Add(Motion);
				const turtlesim::TurnResult Result = turtlesim::ResolveTurn(State.Board, UGatherAndHuntPuzzleSession::ConvertTurnPlan(Plan), NoPlan);

				const turtlesim::Turtle* const Ship = Result.FinalState.FindTurtle(ShipId);
				if (Ship == nullptr || !Ship->bAlive || Ship->bDelivered)
				{
					continue;
				}

				int32 LastNode = State.LastNode;
				for (const turtlesim::TickResult& Tick : Result.Ticks)
				{
					for (const turtlesim::TickEvent& Event : Tick.Events)
					{
						const int32 Node = Event.Type == turtlesim::EEventType::TurtleClaimedObjective && Event.PrimaryId == ShipId
							? MarkerObjectiveIds.IndexOfByKey(Event.SecondaryId)
							: INDEX_NONE;
						LastNode = Node != INDEX_NONE ? Node : LastNode;
					}
				}
				if (LastNode != INDEX_NONE)
				{
					Reachable.Add(LastNode);
				}

				const int32 Facing = static_cast<int32>(Ship->Facing.Heading) * 3 + static_cast<int32>(Ship->Facing.Pitch);
				bool bAlreadySeen = false;
				Seen.Add(MakeTuple(UGatherAndHuntPuzzleSession::ToFIntVector(Ship->Position), Facing, LastNode), &bAlreadySeen);
				if (!bAlreadySeen)
				{
					Next.Add({Result.FinalState, LastNode});
				}
			}
		}
		Frontier = MoveTemp(Next);
	}
	return Reachable;
}

bool UGatherAndHuntCampaign::Validate(TArray<FString>& OutIssues)
{
	OutIssues.Reset();
	if (Definition.StartingMoves <= 0)
	{
		OutIssues.Add(TEXT("StartingMoves is 0: the ship can't leave its starting cell."));
	}
	if (!IsGatherAndHuntMoveAllowed(MotionMask(Definition.StartingMotions), EGatherAndHuntPuzzleMoveKind::Forward))
	{
		OutIssues.Add(TEXT("StartingMotions doesn't include Forward: the ship can't leave its starting cell."));
	}

	FString Error;
	ULevelSave* const Map = BuildMapLevelSave(GetTransientPackage(), Error);
	UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
	const bool bHasMap = Map != nullptr && Session->LoadPuzzleFromLevelSave(Map);
	if (!bHasMap)
	{
		OutIssues.Add(Map == nullptr ? Error : FString(TEXT("The map board doesn't convert to a playable board.")));
	}

	// What already stands on the map board, by cell.
	TMap<FIntVector, FString> Occupied;
	FIntVector BoardSize = FIntVector::ZeroValue;
	if (bHasMap)
	{
		const FGatherAndHuntPuzzleBoardSnapshot Snapshot = Session->GetCurrentBoardSnapshot();
		BoardSize = Snapshot.GridSize;
		bool bHasShip = false;
		for (const FGatherAndHuntPuzzleTurtleState& Turtle : Snapshot.Turtles)
		{
			bHasShip |= Turtle.Role == EGatherAndHuntTurtleRole::Player;
			Occupied.Add(Turtle.GridPosition, TEXT("a ship"));
		}
		for (const FGatherAndHuntPuzzleObstacleState& Obstacle : Snapshot.Obstacles)
		{
			Occupied.Add(Obstacle.GridPosition, TEXT("a block or hazard"));
		}
		if (!bHasShip)
		{
			OutIssues.Add(TEXT("The map board has no player ship."));
		}
	}

	TSet<FString> SeenIds;
	for (const FGatherAndHuntCampaignNode& Node : Definition.Nodes)
	{
		const FString Name = FString::Printf(TEXT("Node \"%s\""), *Node.Id);
		if (!IsValidNodeId(Node.Id))
		{
			OutIssues.Add(Name + TEXT(": the id must be letters, digits, _ or -."));
		}
		bool bDuplicate = false;
		SeenIds.Add(Node.Id, &bDuplicate);
		if (bDuplicate)
		{
			OutIssues.Add(Name + TEXT(": the id is used more than once."));
		}
		if (!UGatherAndHuntFrontEnd::PuzzleFileExists(Node.Puzzle))
		{
			OutIssues.Add(FString::Printf(TEXT("%s: puzzle file \"%s\" doesn't exist."), *Name, *Node.Puzzle));
		}
		if (!bHasMap)
		{
			continue;
		}
		if (Node.Cell.X < 0 || Node.Cell.Y < 0 || Node.Cell.Z < 0 || Node.Cell.X >= BoardSize.X || Node.Cell.Y >= BoardSize.Y || Node.Cell.Z >= BoardSize.Z)
		{
			OutIssues.Add(FString::Printf(TEXT("%s: cell (%d,%d,%d) is off the %dx%dx%d map."), *Name,
				Node.Cell.X, Node.Cell.Y, Node.Cell.Z, BoardSize.X, BoardSize.Y, BoardSize.Z));
		}
		else if (const FString* const Other = Occupied.Find(Node.Cell))
		{
			OutIssues.Add(FString::Printf(TEXT("%s: cell (%d,%d,%d) is taken by %s."), *Name, Node.Cell.X, Node.Cell.Y, Node.Cell.Z, **Other));
		}
		Occupied.Add(Node.Cell, TEXT("another marker"));
	}
	return OutIssues.Num() == 0;
}

const TCHAR* UGatherAndHuntCampaign::MotionLabel(const EGatherAndHuntPuzzleMoveKind Motion)
{
	switch (Motion)
	{
	case EGatherAndHuntPuzzleMoveKind::Forward: return TEXT("FWD");
	case EGatherAndHuntPuzzleMoveKind::YawLeft: return TEXT("YAW L");
	case EGatherAndHuntPuzzleMoveKind::YawRight: return TEXT("YAW R");
	case EGatherAndHuntPuzzleMoveKind::PitchUp: return TEXT("UP");
	case EGatherAndHuntPuzzleMoveKind::PitchDown: return TEXT("DOWN");
	case EGatherAndHuntPuzzleMoveKind::Wait: return TEXT("WAIT");
	}
	return TEXT("?");
}

FString UGatherAndHuntCampaign::DescribeMotions(const int32 MoveMask)
{
	TArray<FString> Parts;
	for (const EGatherAndHuntPuzzleMoveKind Motion : AllMotions)
	{
		if (IsGatherAndHuntMoveAllowed(MoveMask, Motion))
		{
			Parts.Add(MotionLabel(Motion));
		}
	}
	return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : FString(TEXT("none"));
}

FString UGatherAndHuntCampaign::DescribeReward(const FGatherAndHuntCampaignReward& Reward)
{
	TArray<FString> Parts;
	if (Reward.Moves != 0)
	{
		Parts.Add(FString::Printf(TEXT("%+d %s"), Reward.Moves, FMath::Abs(Reward.Moves) == 1 ? TEXT("move") : TEXT("moves")));
	}
	if (Reward.Motions.Num() > 0)
	{
		Parts.Add(DescribeMotions(MotionMask(Reward.Motions)));
	}
	return FString::Join(Parts, TEXT(", "));
}
