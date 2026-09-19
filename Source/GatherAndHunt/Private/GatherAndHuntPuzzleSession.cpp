#include "GatherAndHuntPuzzleSession.h"

#include "Constants.h"
#include "GatherAndHuntLevelEditorLibrary.h"

#include <algorithm>

namespace
{
	using namespace turtlesim;

	EHeading HeadingFromHorizontalVector(const FVector& Vector)
	{
		const float AbsX = FMath::Abs(Vector.X);
		const float AbsY = FMath::Abs(Vector.Y);
		if (AbsX >= AbsY)
		{
			return Vector.X >= 0.0f ? EHeading::East : EHeading::West;
		}
		return Vector.Y >= 0.0f ? EHeading::North : EHeading::South;
	}

	int32 GetObjectivePointValueForPiece(const PieceTypeEnum PieceType)
	{
		switch (PieceType)
		{
		case PieceTypeEnum::BP_GoldObjective:
			return 5;
		case PieceTypeEnum::BP_Objective:
		default:
			return 1;
		}
	}

	bool IsObjectivePiece(const PieceTypeEnum PieceType)
	{
		return PieceType == PieceTypeEnum::BP_Objective || PieceType == PieceTypeEnum::BP_GoldObjective;
	}

	bool IsPlayerPiece(const PieceTypeEnum PieceType)
	{
		return PieceType == PieceTypeEnum::BP_PlayerOwnedPiece;
	}

	bool IsDeterministicHazardPiece(const FPieceSaveData& PieceData)
	{
		return PieceData.IsDeterministicHazard();
	}

	bool IsHunterHazardPiece(const FPieceSaveData& PieceData)
	{
		return PieceData.MovementType == MovementTypeEnum::BP_ChaseNearest
			|| PieceData.MovementType == MovementTypeEnum::BP_ChaseTarget;
	}

	turtlesim::EMoveKind ConvertMoveKind(const EGatherAndHuntPuzzleMoveKind MoveKind)
	{
		switch (MoveKind)
		{
		case EGatherAndHuntPuzzleMoveKind::Forward:
			return turtlesim::EMoveKind::Forward;
		case EGatherAndHuntPuzzleMoveKind::YawLeft:
			return turtlesim::EMoveKind::YawLeft;
		case EGatherAndHuntPuzzleMoveKind::YawRight:
			return turtlesim::EMoveKind::YawRight;
		case EGatherAndHuntPuzzleMoveKind::PitchUp:
			return turtlesim::EMoveKind::PitchUp;
		case EGatherAndHuntPuzzleMoveKind::PitchDown:
			return turtlesim::EMoveKind::PitchDown;
		case EGatherAndHuntPuzzleMoveKind::Wait:
		default:
			return turtlesim::EMoveKind::Wait;
		}
	}
}

bool UGatherAndHuntPuzzleSession::LoadPuzzleFromRelativeFilename(const FString& RelativeFilename)
{
	ULevelSave* const Loaded = UGatherAndHuntLevelEditorLibrary::LoadLevelSaveFromProjectFile(this, RelativeFilename);
	return Loaded != nullptr && LoadPuzzleFromLevelSave(Loaded);
}

bool UGatherAndHuntPuzzleSession::LoadPuzzleFromLevelSave(ULevelSave* LevelSave)
{
	if (LevelSave == nullptr)
	{
		return false;
	}

	turtlesim::BoardState ConvertedBoardState;
	if (!ConvertLevelSaveToBoardState(LevelSave, ConvertedBoardState))
	{
		return false;
	}

	LoadedLevelSave = DuplicateObject<ULevelSave>(LevelSave, this);
	InitialBoardState = ConvertedBoardState;
	CurrentBoardState = ConvertedBoardState;
	LastPlaybackStartBoardState = turtlesim::BoardState{};
	LastTurnResult = turtlesim::TurnResult{};
	bHasCurrentBoardState = true;
	bHasLastPlaybackStartBoardState = false;
	bHasLastTurnResult = false;
	Outcome = EGatherAndHuntPuzzleOutcome::InProgress;
	bHasUnsavedChanges = false;
	EvaluateOutcome();
	return true;
}

bool UGatherAndHuntPuzzleSession::ResetToInitialState()
{
	if (LoadedLevelSave == nullptr)
	{
		return false;
	}

	CurrentBoardState = InitialBoardState;
	LastPlaybackStartBoardState = turtlesim::BoardState{};
	LastTurnResult = turtlesim::TurnResult{};
	bHasCurrentBoardState = true;
	bHasLastPlaybackStartBoardState = false;
	bHasLastTurnResult = false;
	Outcome = EGatherAndHuntPuzzleOutcome::InProgress;
	EvaluateOutcome();
	return true;
}

bool UGatherAndHuntPuzzleSession::SubmitSoloTurnPlan(
	const FGatherAndHuntPuzzleTurnPlan& TurnPlan,
	FGatherAndHuntPuzzlePlanValidation& OutValidation)
{
	OutValidation = FGatherAndHuntPuzzlePlanValidation{};

	if (!IsPuzzleLoaded() || Outcome == EGatherAndHuntPuzzleOutcome::Victory || Outcome == EGatherAndHuntPuzzleOutcome::Defeat)
	{
		OutValidation.bIsValid = false;
		OutValidation.ErrorMessage = TEXT("Puzzle session is not accepting turns.");
		return false;
	}

	const turtlesim::TurnPlan PlayerPlan = ConvertTurnPlan(TurnPlan);
	const turtlesim::PlanValidation Validation = turtlesim::ValidateTurnPlan(
		CurrentBoardState,
		PlayerPlan,
		turtlesim::DefaultMoveBudget);
	OutValidation = BuildValidation(Validation);
	if (!OutValidation.bIsValid)
	{
		return false;
	}

	const turtlesim::TurnPlan OpponentPlan = BuildOpponentPlanForCurrentTurn();

	LastPlaybackStartBoardState = CurrentBoardState;
	bHasLastPlaybackStartBoardState = true;
	LastTurnResult = turtlesim::ResolveTurn(CurrentBoardState, PlayerPlan, OpponentPlan);
	CurrentBoardState = LastTurnResult.FinalState;
	bHasCurrentBoardState = true;
	bHasLastTurnResult = true;
	EvaluateOutcome();
	return true;
}

turtlesim::TurnPlan UGatherAndHuntPuzzleSession::BuildOpponentPlanForCurrentTurn() const
{
	turtlesim::TurnPlan OpponentPlan;
	OpponentPlan.Player = 1;
	if (!bHasCurrentBoardState)
	{
		return OpponentPlan;
	}

	if (const FScriptedOpponentTurnData* ScriptedTurn = FindScriptedOpponentTurn(LoadedLevelSave, CurrentBoardState.TurnNumber))
	{
		OpponentPlan = ConvertScriptedOpponentTurn(*ScriptedTurn);
	}
	return OpponentPlan;
}

bool UGatherAndHuntPuzzleSession::HasScriptedOpponentTurn(const int32 TurnNumber) const
{
	return FindScriptedOpponentTurn(LoadedLevelSave, TurnNumber) != nullptr;
}

FGatherAndHuntPuzzleTurnPlan UGatherAndHuntPuzzleSession::GetScriptedOpponentTurn(const int32 TurnNumber) const
{
	FGatherAndHuntPuzzleTurnPlan Plan;
	if (const FScriptedOpponentTurnData* const TurnData = FindScriptedOpponentTurn(LoadedLevelSave, TurnNumber))
	{
		for (const FScriptedPuzzleTurtlePlanData& TurtlePlanData : TurnData->TurtlePlans)
		{
			FGatherAndHuntPuzzleTurtlePlan& TurtlePlan = Plan.TurtlePlans.AddDefaulted_GetRef();
			TurtlePlan.TurtleId = TurtlePlanData.TurtleId;
			TurtlePlan.Moves = TurtlePlanData.Moves;
		}
	}
	return Plan;
}

bool UGatherAndHuntPuzzleSession::SetScriptedOpponentTurn(const int32 TurnNumber, const FGatherAndHuntPuzzleTurnPlan& TurnPlan)
{
	if (LoadedLevelSave == nullptr || TurnNumber < 0)
	{
		return false;
	}

	TArray<FScriptedOpponentTurnData>& Turns = LoadedLevelSave->ScriptedOpponentTurns;
	Turns.RemoveAll([TurnNumber](const FScriptedOpponentTurnData& TurnData) { return TurnData.TurnNumber == TurnNumber; });

	FScriptedOpponentTurnData NewTurn;
	NewTurn.TurnNumber = TurnNumber;
	for (const FGatherAndHuntPuzzleTurtlePlan& TurtlePlan : TurnPlan.TurtlePlans)
	{
		if (TurtlePlan.Moves.Num() > 0)
		{
			FScriptedPuzzleTurtlePlanData& Data = NewTurn.TurtlePlans.AddDefaulted_GetRef();
			Data.TurtleId = TurtlePlan.TurtleId;
			Data.Moves = TurtlePlan.Moves;
		}
	}
	if (NewTurn.TurtlePlans.Num() > 0)
	{
		Turns.Add(MoveTemp(NewTurn));
		Turns.Sort([](const FScriptedOpponentTurnData& A, const FScriptedOpponentTurnData& B) { return A.TurnNumber < B.TurnNumber; });
	}
	bHasUnsavedChanges = true;
	return true;
}

bool UGatherAndHuntPuzzleSession::SavePuzzleToProjectFile(const FString& RelativeFilename)
{
	if (LoadedLevelSave == nullptr || RelativeFilename.IsEmpty()
		|| !UGatherAndHuntLevelEditorLibrary::SaveLevelSaveToProjectFile(LoadedLevelSave, RelativeFilename))
	{
		return false;
	}
	bHasUnsavedChanges = false;
	return true;
}

const FString& UGatherAndHuntPuzzleSession::GetLoadedPuzzleName() const
{
	static const FString EmptyName;
	return LoadedLevelSave != nullptr ? LoadedLevelSave->LevelName : EmptyName;
}

int32 UGatherAndHuntPuzzleSession::GetCurrentTurnNumber() const
{
	return bHasCurrentBoardState ? CurrentBoardState.TurnNumber : 0;
}

int32 UGatherAndHuntPuzzleSession::GetPlayerScore() const
{
	return bHasCurrentBoardState ? CurrentBoardState.ScorePlayer0 : 0;
}

int32 UGatherAndHuntPuzzleSession::GetTargetScore() const
{
	return LoadedLevelSave != nullptr ? LoadedLevelSave->TargetScore : 0;
}

int32 UGatherAndHuntPuzzleSession::GetTargetTurnCount() const
{
	return LoadedLevelSave != nullptr ? LoadedLevelSave->TargetTurnCount : 0;
}

int32 UGatherAndHuntPuzzleSession::GetTargetObjectiveCount() const
{
	return LoadedLevelSave != nullptr ? LoadedLevelSave->TargetObjectiveCount : 0;
}

const FString& UGatherAndHuntPuzzleSession::GetIntroText() const
{
	static const FString EmptyText;
	return LoadedLevelSave != nullptr ? LoadedLevelSave->IntroText : EmptyText;
}

FGatherAndHuntPuzzleBoardSnapshot UGatherAndHuntPuzzleSession::GetCurrentBoardSnapshot() const
{
	return bHasCurrentBoardState ? BuildBoardSnapshot(CurrentBoardState) : FGatherAndHuntPuzzleBoardSnapshot{};
}

FGatherAndHuntPuzzleBoardSnapshot UGatherAndHuntPuzzleSession::GetLastPlaybackStartBoardSnapshot() const
{
	return bHasLastPlaybackStartBoardState
		? BuildBoardSnapshot(LastPlaybackStartBoardState)
		: FGatherAndHuntPuzzleBoardSnapshot{};
}

FGatherAndHuntPuzzleTurnResultView UGatherAndHuntPuzzleSession::GetLastTurnResultView() const
{
	return bHasLastTurnResult ? BuildTurnResultView(LastTurnResult) : FGatherAndHuntPuzzleTurnResultView{};
}

bool UGatherAndHuntPuzzleSession::ConvertLevelSaveToBoardState(const ULevelSave* LevelSave, turtlesim::BoardState& OutBoardState)
{
	if (LevelSave == nullptr || LevelSave->GridCellSize <= 0.0f)
	{
		return false;
	}

	OutBoardState = turtlesim::BoardState{};
	OutBoardState.Size = turtlesim::IntVec3(
		FMath::Max(1, FMath::RoundToInt(LevelSave->GridSize.X / LevelSave->GridCellSize) + 1),
		FMath::Max(1, FMath::RoundToInt(LevelSave->GridSize.Y / LevelSave->GridCellSize) + 1),
		FMath::Max(1, FMath::RoundToInt(LevelSave->GridSize.Z / LevelSave->GridCellSize) + 1));

	for (const FPieceSaveData& PieceData : LevelSave->PieceSaveDataArray)
	{
		if (IsPlayerPiece(PieceData.PieceType))
		{
			turtlesim::Turtle Turtle;
			Turtle.Id = PieceData.PieceSaveID;
			Turtle.Owner = PieceData.PieceOwnerID;
			Turtle.Position = ConvertWorldPositionToGridPosition(PieceData.Position, LevelSave->GridCellSize);
			Turtle.Facing = ConvertWorldRotationToOrientation(PieceData.Rotation);
			OutBoardState.Turtles.push_back(Turtle);
			continue;
		}

		if (IsObjectivePiece(PieceData.PieceType))
		{
			turtlesim::Objective Objective;
			Objective.Id = PieceData.PieceSaveID;
			Objective.Position = ConvertWorldPositionToGridPosition(PieceData.Position, LevelSave->GridCellSize);
			Objective.PointValue = GetObjectivePointValueForPiece(PieceData.PieceType);
			OutBoardState.Objectives.push_back(Objective);
			continue;
		}

		turtlesim::Obstacle Obstacle;
		Obstacle.Id = PieceData.PieceSaveID;
		Obstacle.Position = ConvertWorldPositionToGridPosition(PieceData.Position, LevelSave->GridCellSize);
		Obstacle.OriginPosition = Obstacle.Position;

		if (IsDeterministicHazardPiece(PieceData))
		{
			Obstacle.Kind = turtlesim::EObstacleKind::Wandering;
			Obstacle.Pattern = PieceData.IsCircularHazard()
				? turtlesim::EWanderingPattern::CircularLoop
				: turtlesim::EWanderingPattern::LinearPatrol;
			Obstacle.PatternOrientation = ConvertWorldRotationToOrientation(PieceData.Rotation);
			Obstacle.StepDistance = ConvertHazardStepDistanceToCells(PieceData.HazardStepDistance, LevelSave->GridCellSize);
			Obstacle.PrimaryStepCount = FMath::Max(1, PieceData.HazardPrimaryStepCount);
			Obstacle.SecondaryStepCount = FMath::Max(1, PieceData.HazardSecondaryStepCount);
			Obstacle.PhaseOffset = PieceData.HazardPhaseOffset;
		}
		else if (IsHunterHazardPiece(PieceData))
		{
			Obstacle.Kind = turtlesim::EObstacleKind::Hunter;
		}
		else
		{
			Obstacle.Kind = turtlesim::EObstacleKind::Static;
		}

		OutBoardState.Obstacles.push_back(Obstacle);
	}

	std::sort(OutBoardState.Turtles.begin(), OutBoardState.Turtles.end(),
		[](const turtlesim::Turtle& A, const turtlesim::Turtle& B)
		{
			return A.Id < B.Id;
		});
	std::sort(OutBoardState.Obstacles.begin(), OutBoardState.Obstacles.end(),
		[](const turtlesim::Obstacle& A, const turtlesim::Obstacle& B)
		{
			return A.Id < B.Id;
		});
	std::sort(OutBoardState.Objectives.begin(), OutBoardState.Objectives.end(),
		[](const turtlesim::Objective& A, const turtlesim::Objective& B)
		{
			return A.Id < B.Id;
		});

	return true;
}

turtlesim::TurnPlan UGatherAndHuntPuzzleSession::ConvertTurnPlan(const FGatherAndHuntPuzzleTurnPlan& TurnPlan)
{
	turtlesim::TurnPlan ConvertedPlan;
	ConvertedPlan.Player = 0;
	ConvertedPlan.PerTurtle.reserve(TurnPlan.TurtlePlans.Num());

	for (const FGatherAndHuntPuzzleTurtlePlan& TurtlePlan : TurnPlan.TurtlePlans)
	{
		turtlesim::TurtlePlan ConvertedTurtlePlan;
		ConvertedTurtlePlan.Turtle = TurtlePlan.TurtleId;
		ConvertedTurtlePlan.Moves.reserve(TurtlePlan.Moves.Num());

		for (EGatherAndHuntPuzzleMoveKind MoveKind : TurtlePlan.Moves)
		{
			ConvertedTurtlePlan.Moves.push_back(turtlesim::Move{ConvertMoveKind(MoveKind)});
		}

		ConvertedPlan.PerTurtle.push_back(std::move(ConvertedTurtlePlan));
	}

	return ConvertedPlan;
}

turtlesim::TurnPlan UGatherAndHuntPuzzleSession::ConvertScriptedOpponentTurn(const FScriptedOpponentTurnData& TurnData)
{
	turtlesim::TurnPlan ConvertedPlan;
	ConvertedPlan.Player = 1;
	ConvertedPlan.PerTurtle.reserve(TurnData.TurtlePlans.Num());

	for (const FScriptedPuzzleTurtlePlanData& TurtlePlanData : TurnData.TurtlePlans)
	{
		turtlesim::TurtlePlan TurtlePlan;
		TurtlePlan.Turtle = TurtlePlanData.TurtleId;
		TurtlePlan.Moves.reserve(TurtlePlanData.Moves.Num());

		for (const EGatherAndHuntPuzzleMoveKind MoveKind : TurtlePlanData.Moves)
		{
			TurtlePlan.Moves.push_back(turtlesim::Move{ConvertMoveKind(MoveKind)});
		}

		ConvertedPlan.PerTurtle.push_back(std::move(TurtlePlan));
	}

	return ConvertedPlan;
}

const FScriptedOpponentTurnData* UGatherAndHuntPuzzleSession::FindScriptedOpponentTurn(const ULevelSave* LevelSave, const int32 TurnNumber)
{
	if (LevelSave == nullptr)
	{
		return nullptr;
	}

	for (const FScriptedOpponentTurnData& TurnData : LevelSave->ScriptedOpponentTurns)
	{
		if (TurnData.TurnNumber == TurnNumber)
		{
			return &TurnData;
		}
	}

	return nullptr;
}

FIntVector UGatherAndHuntPuzzleSession::ToFIntVector(const turtlesim::IntVec3& Position)
{
	return FIntVector(Position.X, Position.Y, Position.Z);
}

EGatherAndHuntPuzzleEventType UGatherAndHuntPuzzleSession::ToPuzzleEventType(turtlesim::EEventType EventType)
{
	switch (EventType)
	{
	case turtlesim::EEventType::TurtleTurned: return EGatherAndHuntPuzzleEventType::TurtleTurned;
	case turtlesim::EEventType::TurtleWaited: return EGatherAndHuntPuzzleEventType::TurtleWaited;
	case turtlesim::EEventType::TurtleBlocked: return EGatherAndHuntPuzzleEventType::TurtleBlocked;
	case turtlesim::EEventType::TurtleMoved: return EGatherAndHuntPuzzleEventType::TurtleMoved;
	case turtlesim::EEventType::TurtleDestroyed: return EGatherAndHuntPuzzleEventType::TurtleDestroyed;
	case turtlesim::EEventType::TurtleDelivered: return EGatherAndHuntPuzzleEventType::TurtleDelivered;
	case turtlesim::EEventType::TurtleClaimedObjective: return EGatherAndHuntPuzzleEventType::TurtleClaimedObjective;
	case turtlesim::EEventType::ObstacleMoved: return EGatherAndHuntPuzzleEventType::ObstacleMoved;
	case turtlesim::EEventType::ObstacleDestroyed: return EGatherAndHuntPuzzleEventType::ObstacleDestroyed;
	case turtlesim::EEventType::HunterRetargeted:
	default:
		return EGatherAndHuntPuzzleEventType::HunterRetargeted;
	}
}

FGatherAndHuntPuzzleBoardSnapshot UGatherAndHuntPuzzleSession::BuildBoardSnapshot(const turtlesim::BoardState& BoardState)
{
	FGatherAndHuntPuzzleBoardSnapshot Snapshot;
	Snapshot.GridSize = ToFIntVector(BoardState.Size);
	Snapshot.TurnNumber = BoardState.TurnNumber;
	Snapshot.ScorePlayer0 = BoardState.ScorePlayer0;
	Snapshot.ScorePlayer1 = BoardState.ScorePlayer1;

	for (const turtlesim::Turtle& Turtle : BoardState.Turtles)
	{
		FGatherAndHuntPuzzleTurtleState TurtleState;
		TurtleState.TurtleId = Turtle.Id;
		TurtleState.OwnerId = Turtle.Owner;
		TurtleState.GridPosition = ToFIntVector(Turtle.Position);
		TurtleState.Heading = static_cast<int32>(Turtle.Facing.Heading);
		TurtleState.Pitch = static_cast<int32>(Turtle.Facing.Pitch);
		TurtleState.bAlive = Turtle.bAlive;
		TurtleState.bDelivered = Turtle.bDelivered;
		TurtleState.CarriedPoints = Turtle.CarriedPoints;
		Snapshot.Turtles.Add(TurtleState);
	}

	for (const turtlesim::Obstacle& Obstacle : BoardState.Obstacles)
	{
		FGatherAndHuntPuzzleObstacleState ObstacleState;
		ObstacleState.ObstacleId = Obstacle.Id;
		ObstacleState.ObstacleKind = static_cast<int32>(Obstacle.Kind);
		ObstacleState.PatternKind = static_cast<int32>(Obstacle.Pattern);
		ObstacleState.GridPosition = ToFIntVector(Obstacle.Position);
		ObstacleState.bAlive = Obstacle.bAlive;
		Snapshot.Obstacles.Add(ObstacleState);
	}

	for (const turtlesim::Objective& Objective : BoardState.Objectives)
	{
		FGatherAndHuntPuzzleObjectiveState ObjectiveState;
		ObjectiveState.ObjectiveId = Objective.Id;
		ObjectiveState.GridPosition = ToFIntVector(Objective.Position);
		ObjectiveState.PointValue = Objective.PointValue;
		ObjectiveState.bClaimed = Objective.bClaimed;
		Snapshot.Objectives.Add(ObjectiveState);
	}

	return Snapshot;
}

FGatherAndHuntPuzzleTurnResultView UGatherAndHuntPuzzleSession::BuildTurnResultView(const turtlesim::TurnResult& TurnResult)
{
	FGatherAndHuntPuzzleTurnResultView View;
	View.FinalBoard = BuildBoardSnapshot(TurnResult.FinalState);

	for (const turtlesim::TickResult& Tick : TurnResult.Ticks)
	{
		FGatherAndHuntPuzzleTickResultView TickView;
		TickView.TickIndex = Tick.TickIndex;

		for (const turtlesim::TickEvent& Event : Tick.Events)
		{
			FGatherAndHuntPuzzleTickEventView EventView;
			EventView.Type = ToPuzzleEventType(Event.Type);
			EventView.PrimaryId = Event.PrimaryId;
			EventView.SecondaryId = Event.SecondaryId;
			EventView.FromPosition = ToFIntVector(Event.FromPosition);
			EventView.ToPosition = ToFIntVector(Event.ToPosition);
			EventView.FromHeading = static_cast<int32>(Event.FromOrientation.Heading);
			EventView.FromPitch = static_cast<int32>(Event.FromOrientation.Pitch);
			EventView.ToHeading = static_cast<int32>(Event.ToOrientation.Heading);
			EventView.ToPitch = static_cast<int32>(Event.ToOrientation.Pitch);
			EventView.PointsDelta = Event.PointsDelta;
			TickView.Events.Add(EventView);
		}

		View.Ticks.Add(TickView);
	}

	return View;
}

FGatherAndHuntPuzzlePlanValidation UGatherAndHuntPuzzleSession::BuildValidation(const turtlesim::PlanValidation& Validation)
{
	FGatherAndHuntPuzzlePlanValidation Result;
	Result.bIsValid = Validation.Error == turtlesim::EPlanValidationError::None;
	Result.OffendingTurtleId = Validation.OffendingTurtle;
	Result.OffendingMoveIndex = Validation.OffendingMoveIndex;

	switch (Validation.Error)
	{
	case turtlesim::EPlanValidationError::None:
		break;
	case turtlesim::EPlanValidationError::BudgetExceeded:
		Result.ErrorMessage = TEXT("Turn plan exceeds the move budget.");
		break;
	case turtlesim::EPlanValidationError::TurtleNotOwned:
		Result.ErrorMessage = TEXT("Turn plan contains a turtle not owned by the player.");
		break;
	case turtlesim::EPlanValidationError::TurtleNotAlive:
		Result.ErrorMessage = TEXT("Turn plan contains a turtle that is not active.");
		break;
	case turtlesim::EPlanValidationError::TurtleUnknown:
		Result.ErrorMessage = TEXT("Turn plan references an unknown turtle.");
		break;
	case turtlesim::EPlanValidationError::DuplicateTurtleEntry:
		Result.ErrorMessage = TEXT("Turn plan contains duplicate turtle entries.");
		break;
	case turtlesim::EPlanValidationError::InvalidPitchTransition:
		Result.ErrorMessage = TEXT("Turn plan contains an invalid pitch transition.");
		break;
	case turtlesim::EPlanValidationError::InvalidMoveKind:
	default:
		Result.ErrorMessage = TEXT("Turn plan contains an invalid move.");
		break;
	}

	return Result;
}

turtlesim::Orientation UGatherAndHuntPuzzleSession::ConvertWorldRotationToOrientation(const FRotator& Rotation)
{
	const FVector Forward = Rotation.Vector().GetSafeNormal();
	const float AbsX = FMath::Abs(Forward.X);
	const float AbsY = FMath::Abs(Forward.Y);
	const float AbsZ = FMath::Abs(Forward.Z);
	const turtlesim::EHeading Heading = HeadingFromHorizontalVector(FRotationMatrix(Rotation).GetScaledAxis(EAxis::X));

	if (AbsZ > AbsX && AbsZ > AbsY)
	{
		return turtlesim::Orientation(Heading, Forward.Z >= 0.0f ? turtlesim::EPitch::Up : turtlesim::EPitch::Down);
	}

	return turtlesim::Orientation(HeadingFromHorizontalVector(Forward), turtlesim::EPitch::Level);
}

turtlesim::IntVec3 UGatherAndHuntPuzzleSession::ConvertWorldPositionToGridPosition(const FVector& Position, const float GridCellSize)
{
	return turtlesim::IntVec3(
		FMath::RoundToInt(Position.X / GridCellSize),
		FMath::RoundToInt(Position.Y / GridCellSize),
		FMath::RoundToInt(Position.Z / GridCellSize));
}

int32 UGatherAndHuntPuzzleSession::ConvertHazardStepDistanceToCells(const float HazardStepDistance, const float GridCellSize)
{
	return FMath::Max(1, FMath::RoundToInt(HazardStepDistance / GridCellSize));
}

void UGatherAndHuntPuzzleSession::EvaluateOutcome()
{
	if (!bHasCurrentBoardState || LoadedLevelSave == nullptr)
	{
		Outcome = EGatherAndHuntPuzzleOutcome::NotLoaded;
		return;
	}

	const int32 ClaimedObjectives = static_cast<int32>(std::count_if(
		CurrentBoardState.Objectives.begin(),
		CurrentBoardState.Objectives.end(),
		[](const turtlesim::Objective& Objective)
		{
			return Objective.bClaimed;
		}));

	bool bHasLiveCarrier = false;
	bool bHasLivingPlayerTurtle = false;
	for (const turtlesim::Turtle& Turtle : CurrentBoardState.Turtles)
	{
		if (Turtle.Owner != 0)
		{
			continue;
		}

		if (Turtle.bAlive && !Turtle.bDelivered)
		{
			bHasLivingPlayerTurtle = true;
			if (Turtle.CarriedPoints > 0)
			{
				bHasLiveCarrier = true;
			}
		}
	}

	const bool bTargetScoreReached = LoadedLevelSave->TargetScore > 0
		&& CurrentBoardState.ScorePlayer0 >= LoadedLevelSave->TargetScore;
	const bool bTargetObjectivesReached = LoadedLevelSave->TargetObjectiveCount > 0
		&& ClaimedObjectives >= LoadedLevelSave->TargetObjectiveCount
		&& !bHasLiveCarrier;
	const bool bAllObjectivesResolved = !CurrentBoardState.Objectives.empty()
		&& ClaimedObjectives == static_cast<int32>(CurrentBoardState.Objectives.size())
		&& !bHasLiveCarrier;

	if (bTargetScoreReached || bTargetObjectivesReached || bAllObjectivesResolved)
	{
		Outcome = EGatherAndHuntPuzzleOutcome::Victory;
		return;
	}

	const bool bTurnLimitReached = LoadedLevelSave->TargetTurnCount > 0
		&& CurrentBoardState.TurnNumber >= LoadedLevelSave->TargetTurnCount;
	if (bTurnLimitReached || !bHasLivingPlayerTurtle)
	{
		Outcome = EGatherAndHuntPuzzleOutcome::Defeat;
		return;
	}

	Outcome = EGatherAndHuntPuzzleOutcome::InProgress;
}
