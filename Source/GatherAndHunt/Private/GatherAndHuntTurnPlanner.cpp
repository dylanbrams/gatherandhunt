#include "GatherAndHuntTurnPlanner.h"

#include "Engine/World.h"
#include "GatherAndHuntPuzzleGameMode.h"
#include "GatherAndHuntPuzzleSession.h"

#include "Constants.h"
#include "CostModel.h"
#include "PlanPreview.h"
#include "Resolver.h"
#include "Validation.h"

namespace
{
	EGatherAndHuntPlannerStepKind ToPlannerStepKind(const turtlesim::ETrackStepKind Kind)
	{
		switch (Kind)
		{
		case turtlesim::ETrackStepKind::Start: return EGatherAndHuntPlannerStepKind::Start;
		case turtlesim::ETrackStepKind::Moved: return EGatherAndHuntPlannerStepKind::Moved;
		case turtlesim::ETrackStepKind::Turned: return EGatherAndHuntPlannerStepKind::Turned;
		case turtlesim::ETrackStepKind::Waited: return EGatherAndHuntPlannerStepKind::Waited;
		case turtlesim::ETrackStepKind::Blocked: return EGatherAndHuntPlannerStepKind::Blocked;
		case turtlesim::ETrackStepKind::Destroyed: return EGatherAndHuntPlannerStepKind::Destroyed;
		case turtlesim::ETrackStepKind::Delivered: return EGatherAndHuntPlannerStepKind::Delivered;
		}
		return EGatherAndHuntPlannerStepKind::Start;
	}

	bool IsPlayable(const turtlesim::Turtle& Turtle)
	{
		return Turtle.bAlive && !Turtle.bDelivered;
	}
}

UWorld* UGatherAndHuntTurnPlanner::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	const UObject* const Outer = GetOuter();
	return Outer != nullptr ? Outer->GetWorld() : nullptr;
}

void UGatherAndHuntTurnPlanner::BindToSession(UGatherAndHuntPuzzleSession* InSession)
{
	Session = InSession;
	MovesByTurtle.Reset();
	SelectedTurtleId = INDEX_NONE;
	SelectedMoveIndex = INDEX_NONE;
	LoadedTurnNumber = INDEX_NONE;
	bOpponentDraftDirty = false;
	RefreshFromSession();
}

void UGatherAndHuntTurnPlanner::RefreshFromSession()
{
	const turtlesim::BoardState* const SessionBoard = Session != nullptr ? Session->GetCurrentBoardState() : nullptr;
	bHasBoard = SessionBoard != nullptr;
	Board = bHasBoard ? *SessionBoard : turtlesim::BoardState{};

	// Drop queues for turtles that died or delivered since the last refresh.
	for (auto It = MovesByTurtle.CreateIterator(); It; ++It)
	{
		const turtlesim::Turtle* const Turtle = Board.FindTurtle(It.Key());
		if (Turtle == nullptr || !IsPlayable(*Turtle))
		{
			It.RemoveCurrent();
		}
	}

	// A new board turn has its own scripted opponent plan; drop the old draft.
	const int32 TurnNumber = bHasBoard ? Board.TurnNumber : INDEX_NONE;
	if (TurnNumber != LoadedTurnNumber)
	{
		LoadedTurnNumber = TurnNumber;
		LoadOpponentDraftFromSession();
	}

	RebuildEditableTurtles();
	Recompute();
}

void UGatherAndHuntTurnPlanner::RebuildEditableTurtles()
{
	PlayerTurtleIds.Reset();
	for (const turtlesim::Turtle& Turtle : Board.Turtles)
	{
		if (Turtle.Owner == GetEditedOwner() && IsPlayable(Turtle))
		{
			PlayerTurtleIds.Add(Turtle.Id);
		}
	}
	PlayerTurtleIds.Sort();

	if (!PlayerTurtleIds.Contains(SelectedTurtleId))
	{
		SelectedTurtleId = PlayerTurtleIds.Num() > 0 ? PlayerTurtleIds[0] : INDEX_NONE;
		SelectedMoveIndex = INDEX_NONE;
	}
}

void UGatherAndHuntTurnPlanner::LoadOpponentDraftFromSession()
{
	const int32 OpponentOwner = 1 - PlayerId;
	for (auto It = MovesByTurtle.CreateIterator(); It; ++It)
	{
		const turtlesim::Turtle* const Turtle = Board.FindTurtle(It.Key());
		if (Turtle != nullptr && Turtle->Owner == OpponentOwner)
		{
			It.RemoveCurrent();
		}
	}

	if (bHasBoard && Session != nullptr)
	{
		for (const FGatherAndHuntPuzzleTurtlePlan& TurtlePlan : Session->GetScriptedOpponentTurn(Board.TurnNumber).TurtlePlans)
		{
			const turtlesim::Turtle* const Turtle = Board.FindTurtle(TurtlePlan.TurtleId);
			if (Turtle != nullptr && Turtle->Owner == OpponentOwner && IsPlayable(*Turtle) && TurtlePlan.Moves.Num() > 0)
			{
				MovesByTurtle.Add(TurtlePlan.TurtleId, TurtlePlan.Moves);
			}
		}
	}
	bOpponentDraftDirty = false;
}

void UGatherAndHuntTurnPlanner::MarkEdited()
{
	if (bEditingOpponent)
	{
		bOpponentDraftDirty = true;
	}
	Recompute();
}

void UGatherAndHuntTurnPlanner::SetEditingOpponent(const bool bEditOpponent)
{
	if (bEditingOpponent == bEditOpponent)
	{
		return;
	}
	bEditingOpponent = bEditOpponent;
	SelectedTurtleId = INDEX_NONE;
	SelectedMoveIndex = INDEX_NONE;
	RebuildEditableTurtles();
	Recompute();
}

bool UGatherAndHuntTurnPlanner::SaveOpponentTurn(FGatherAndHuntPuzzlePlanValidation& OutValidation)
{
	OutValidation = FGatherAndHuntPuzzlePlanValidation{};
	if (Session == nullptr || !bHasBoard)
	{
		OutValidation.bIsValid = false;
		OutValidation.ErrorMessage = TEXT("No puzzle loaded.");
		return false;
	}

	const FGatherAndHuntPuzzleTurnPlan Draft = BuildPlanForOwner(1 - PlayerId);
	turtlesim::TurnPlan Converted = UGatherAndHuntPuzzleSession::ConvertTurnPlan(Draft);
	Converted.Player = 1 - PlayerId;
	OutValidation = UGatherAndHuntPuzzleSession::BuildValidation(turtlesim::ValidateTurnPlan(Board, Converted, Budget));
	if (!OutValidation.bIsValid || !Session->SetScriptedOpponentTurn(Board.TurnNumber, Draft))
	{
		return false;
	}

	bOpponentDraftDirty = false;
	Recompute();
	return true;
}

void UGatherAndHuntTurnPlanner::RevertOpponentDraft()
{
	LoadOpponentDraftFromSession();
	SelectedMoveIndex = INDEX_NONE;
	Recompute();
}

void UGatherAndHuntTurnPlanner::SelectTurtle(const int32 TurtleId)
{
	if (!PlayerTurtleIds.Contains(TurtleId) || TurtleId == SelectedTurtleId)
	{
		return;
	}
	SelectedTurtleId = TurtleId;
	SelectedMoveIndex = INDEX_NONE;
	OnPlanChanged.Broadcast();
}

void UGatherAndHuntTurnPlanner::SelectAdjacentTurtle(const int32 Direction)
{
	if (PlayerTurtleIds.Num() == 0)
	{
		return;
	}
	const int32 Current = PlayerTurtleIds.IndexOfByKey(SelectedTurtleId);
	const int32 Count = PlayerTurtleIds.Num();
	const int32 Next = Current == INDEX_NONE ? 0 : ((Current + Direction) % Count + Count) % Count;
	SelectTurtle(PlayerTurtleIds[Next]);
}

void UGatherAndHuntTurnPlanner::SelectMove(const int32 MoveIndex)
{
	const TArray<EGatherAndHuntPuzzleMoveKind>* const Moves = FindSelectedMoves();
	const int32 NewIndex = Moves != nullptr && Moves->IsValidIndex(MoveIndex) ? MoveIndex : INDEX_NONE;
	if (NewIndex != SelectedMoveIndex)
	{
		SelectedMoveIndex = NewIndex;
		OnPlanChanged.Broadcast();
	}
}

void UGatherAndHuntTurnPlanner::AddMove(const EGatherAndHuntPuzzleMoveKind MoveKind)
{
	if (SelectedTurtleId == INDEX_NONE)
	{
		return;
	}
	TArray<EGatherAndHuntPuzzleMoveKind>& Moves = MovesByTurtle.FindOrAdd(SelectedTurtleId);
	const int32 InsertAt = Moves.IsValidIndex(SelectedMoveIndex) ? SelectedMoveIndex + 1 : Moves.Num();
	Moves.Insert(MoveKind, InsertAt);
	// Keep the cursor on the new move only when inserting mid-queue; appending
	// leaves it at the end so repeated key presses keep appending.
	SelectedMoveIndex = InsertAt < Moves.Num() - 1 ? InsertAt : INDEX_NONE;
	MarkEdited();
}

void UGatherAndHuntTurnPlanner::RemoveMove(const int32 MoveIndex)
{
	TArray<EGatherAndHuntPuzzleMoveKind>* const Moves = FindSelectedMoves();
	if (Moves == nullptr || !Moves->IsValidIndex(MoveIndex))
	{
		return;
	}
	Moves->RemoveAt(MoveIndex);
	if (SelectedMoveIndex >= Moves->Num())
	{
		SelectedMoveIndex = INDEX_NONE;
	}
	MarkEdited();
}

void UGatherAndHuntTurnPlanner::RemoveSelectedMove()
{
	const TArray<EGatherAndHuntPuzzleMoveKind>* const Moves = FindSelectedMoves();
	if (Moves == nullptr || Moves->Num() == 0)
	{
		return;
	}
	const int32 Index = Moves->IsValidIndex(SelectedMoveIndex) ? SelectedMoveIndex : Moves->Num() - 1;
	RemoveMove(Index);
}

void UGatherAndHuntTurnPlanner::ShiftSelectedMove(const int32 Direction)
{
	TArray<EGatherAndHuntPuzzleMoveKind>* const Moves = FindSelectedMoves();
	if (Moves == nullptr || !Moves->IsValidIndex(SelectedMoveIndex))
	{
		return;
	}
	const int32 Target = SelectedMoveIndex + FMath::Sign(Direction);
	if (!Moves->IsValidIndex(Target))
	{
		return;
	}
	Moves->Swap(SelectedMoveIndex, Target);
	SelectedMoveIndex = Target;
	MarkEdited();
}

void UGatherAndHuntTurnPlanner::ClearSelectedTurtle()
{
	if (MovesByTurtle.Remove(SelectedTurtleId) > 0)
	{
		SelectedMoveIndex = INDEX_NONE;
		MarkEdited();
	}
}

void UGatherAndHuntTurnPlanner::ClearAll()
{
	for (const int32 TurtleId : PlayerTurtleIds)
	{
		MovesByTurtle.Remove(TurtleId);
	}
	SelectedMoveIndex = INDEX_NONE;
	MarkEdited();
}

TArray<EGatherAndHuntPuzzleMoveKind> UGatherAndHuntTurnPlanner::GetTurtleMoves(const int32 TurtleId) const
{
	const TArray<EGatherAndHuntPuzzleMoveKind>* const Moves = MovesByTurtle.Find(TurtleId);
	return Moves != nullptr ? *Moves : TArray<EGatherAndHuntPuzzleMoveKind>();
}

FGatherAndHuntPuzzleTurnPlan UGatherAndHuntTurnPlanner::BuildPlanForOwner(const int32 Owner) const
{
	FGatherAndHuntPuzzleTurnPlan Plan;
	for (const turtlesim::Turtle& Turtle : Board.Turtles)
	{
		const TArray<EGatherAndHuntPuzzleMoveKind>* const Moves = MovesByTurtle.Find(Turtle.Id);
		if (Turtle.Owner == Owner && IsPlayable(Turtle) && Moves != nullptr && Moves->Num() > 0)
		{
			FGatherAndHuntPuzzleTurtlePlan& TurtlePlan = Plan.TurtlePlans.AddDefaulted_GetRef();
			TurtlePlan.TurtleId = Turtle.Id;
			TurtlePlan.Moves = *Moves;
		}
	}
	Plan.TurtlePlans.Sort([](const FGatherAndHuntPuzzleTurtlePlan& A, const FGatherAndHuntPuzzleTurtlePlan& B) { return A.TurtleId < B.TurtleId; });
	return Plan;
}

int32 UGatherAndHuntTurnPlanner::GetMoveCost(const int32 MoveIndex)
{
	return turtlesim::CostModel::MoveCost(MoveIndex + 1);
}

int32 UGatherAndHuntTurnPlanner::GetMoveCostTier(const int32 MoveIndex)
{
	return FMath::Max(1, turtlesim::CostModel::MoveCost(MoveIndex + 1));
}

void UGatherAndHuntTurnPlanner::SetPreviewIncludesOpponent(const bool bInclude)
{
	if (bPreviewIncludesOpponent != bInclude)
	{
		bPreviewIncludesOpponent = bInclude;
		Recompute();
	}
}

bool UGatherAndHuntTurnPlanner::Submit(FGatherAndHuntPuzzlePlanValidation& OutValidation)
{
	OutValidation = Validation;
	if (Session == nullptr || !CanSubmit())
	{
		if (Session == nullptr)
		{
			OutValidation.bIsValid = false;
			OutValidation.ErrorMessage = TEXT("No puzzle session is bound to the planner.");
		}
		return false;
	}

	const FGatherAndHuntPuzzleTurnPlan Plan = BuildPlanForOwner(PlayerId);
	UWorld* const World = GetWorld();
	AGatherAndHuntPuzzleGameMode* const GameMode = World != nullptr ? World->GetAuthGameMode<AGatherAndHuntPuzzleGameMode>() : nullptr;
	const bool bAccepted = GameMode != nullptr && GameMode->GetPuzzleSession() == Session
		? GameMode->SubmitSoloTurnPlan(Plan, OutValidation)
		: Session->SubmitSoloTurnPlan(Plan, OutValidation);

	if (!bAccepted)
	{
		Validation = OutValidation;
		OnPlanChanged.Broadcast();
		return false;
	}

	for (const turtlesim::Turtle& Turtle : Board.Turtles)
	{
		if (Turtle.Owner == PlayerId)
		{
			MovesByTurtle.Remove(Turtle.Id);
		}
	}
	SelectedMoveIndex = INDEX_NONE;
	RefreshFromSession();
	OnPlanSubmitted.Broadcast();
	return true;
}

void UGatherAndHuntTurnPlanner::Recompute()
{
	TotalCost = 0;
	TurtleSummaries.Reset();
	PreviewTracks.Reset();
	PreviewSummary = FGatherAndHuntPlannerPreviewSummary{};
	Validation = FGatherAndHuntPuzzlePlanValidation{};

	if (!bHasBoard)
	{
		Validation.bIsValid = false;
		Validation.ErrorMessage = TEXT("No puzzle loaded.");
		OnPlanChanged.Broadcast();
		return;
	}

	const int32 EditedOwner = GetEditedOwner();
	turtlesim::TurnPlan EditedPlan = UGatherAndHuntPuzzleSession::ConvertTurnPlan(BuildPlanForOwner(EditedOwner));
	EditedPlan.Player = EditedOwner;
	TotalCost = turtlesim::CostModel::TurnCost(EditedPlan);
	Validation = UGatherAndHuntPuzzleSession::BuildValidation(turtlesim::ValidateTurnPlan(Board, EditedPlan, Budget));

	// The other side: the scripted opponent when planning a real turn, or the
	// player's queued plan when authoring the opponent against it.
	turtlesim::TurnPlan OtherPlan;
	OtherPlan.Player = 1 - EditedOwner;
	if (bEditingOpponent)
	{
		OtherPlan = UGatherAndHuntPuzzleSession::ConvertTurnPlan(BuildPlanForOwner(PlayerId));
		OtherPlan.Player = PlayerId;
	}
	else if (bPreviewIncludesOpponent && Session != nullptr)
	{
		OtherPlan = Session->BuildOpponentPlanForCurrentTurn();
	}

	// Preview even when invalid so the ghost path keeps tracking edits; the
	// resolver treats illegal pitch moves as no-ops, and the UI flags the error.
	const turtlesim::TurnResult Result = turtlesim::ResolveTurn(Board, EditedPlan, OtherPlan);

	PreviewSummary.ScoreBefore = EditedOwner == 0 ? Board.ScorePlayer0 : Board.ScorePlayer1;
	PreviewSummary.ScoreAfter = EditedOwner == 0 ? Result.FinalState.ScorePlayer0 : Result.FinalState.ScorePlayer1;
	PreviewSummary.TickCount = static_cast<int32>(Result.Ticks.size());

	for (const turtlesim::Turtle& Turtle : Board.Turtles)
	{
		if (!IsPlayable(Turtle))
		{
			continue;
		}

		FGatherAndHuntPlannerTurtleTrack& Track = PreviewTracks.AddDefaulted_GetRef();
		Track.TurtleId = Turtle.Id;
		Track.OwnerId = Turtle.Owner;
		for (const turtlesim::TrackStep& Step : turtlesim::ExtractTurtleTrack(Board, Result, Turtle.Id))
		{
			FGatherAndHuntPlannerTrackStep& Out = Track.Steps.AddDefaulted_GetRef();
			Out.TickIndex = Step.TickIndex;
			Out.Kind = ToPlannerStepKind(Step.Kind);
			Out.GridPosition = UGatherAndHuntPuzzleSession::ToFIntVector(Step.Position);
			Out.Heading = static_cast<int32>(Step.Facing.Heading);
			Out.Pitch = static_cast<int32>(Step.Facing.Pitch);
			Out.bClaimedObjective = Step.bClaimedObjective;
			Out.PointsDelta = Step.PointsDelta;
		}

		if (Turtle.Owner != EditedOwner)
		{
			continue;
		}

		const EGatherAndHuntPlannerStepKind Fate = Track.Steps.Num() > 0 ? Track.Steps.Last().Kind : EGatherAndHuntPlannerStepKind::Start;
		int32 ClaimedPoints = 0;
		for (const FGatherAndHuntPlannerTrackStep& Step : Track.Steps)
		{
			if (Step.bClaimedObjective)
			{
				++PreviewSummary.ObjectivesClaimed;
				ClaimedPoints += Step.PointsDelta;
			}
		}
		PreviewSummary.TurtlesLost += Fate == EGatherAndHuntPlannerStepKind::Destroyed ? 1 : 0;
		PreviewSummary.TurtlesDelivered += Fate == EGatherAndHuntPlannerStepKind::Delivered ? 1 : 0;

		FGatherAndHuntPlannerTurtleSummary& Summary = TurtleSummaries.AddDefaulted_GetRef();
		Summary.TurtleId = Turtle.Id;
		Summary.GridPosition = UGatherAndHuntPuzzleSession::ToFIntVector(Turtle.Position);
		Summary.Heading = static_cast<int32>(Turtle.Facing.Heading);
		Summary.Pitch = static_cast<int32>(Turtle.Facing.Pitch);
		Summary.CarriedPoints = Turtle.CarriedPoints;
		Summary.PreviewFate = Fate;
		Summary.PreviewClaimedPoints = ClaimedPoints;
		Summary.bHasValidationError = !Validation.bIsValid && Validation.OffendingTurtleId == Turtle.Id;

		if (const TArray<EGatherAndHuntPuzzleMoveKind>* const Moves = MovesByTurtle.Find(Turtle.Id))
		{
			Summary.MoveCount = Moves->Num();
			for (int32 Index = 0; Index < Moves->Num(); ++Index)
			{
				Summary.PlanCost += GetMoveCost(Index);
			}
			Summary.HighestCostTier = Moves->Num() > 0 ? GetMoveCostTier(Moves->Num() - 1) : 0;
		}
	}

	OnPlanChanged.Broadcast();
}

TArray<EGatherAndHuntPuzzleMoveKind>* UGatherAndHuntTurnPlanner::FindSelectedMoves()
{
	return SelectedTurtleId != INDEX_NONE ? MovesByTurtle.Find(SelectedTurtleId) : nullptr;
}
