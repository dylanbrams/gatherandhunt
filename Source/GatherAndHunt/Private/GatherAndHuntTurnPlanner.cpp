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

	turtlesim::TurnPlan ToSimPlan(const FGatherAndHuntPuzzleTurnPlan& Plan, const int32 Player)
	{
		turtlesim::TurnPlan Converted = UGatherAndHuntPuzzleSession::ConvertTurnPlan(Plan);
		Converted.Player = Player;
		return Converted;
	}

	void AppendPlan(turtlesim::TurnPlan& Into, const turtlesim::TurnPlan& From)
	{
		Into.PerTurtle.insert(Into.PerTurtle.end(), From.PerTurtle.begin(), From.PerTurtle.end());
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
	bScriptDraftDirty = false;
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

	// A new board turn has its own scripted turn; drop the old draft.
	const int32 TurnNumber = bHasBoard ? Board.TurnNumber : INDEX_NONE;
	if (TurnNumber != LoadedTurnNumber)
	{
		LoadedTurnNumber = TurnNumber;
		LoadScriptDraftFromSession();
	}

	RebuildEditableTurtles();
	Recompute();
}

EGatherAndHuntTurtleRole UGatherAndHuntTurnPlanner::GetTurtleRole(const int32 TurtleId) const
{
	if (Session != nullptr)
	{
		return Session->GetTurtleRole(TurtleId);
	}
	const turtlesim::Turtle* const Turtle = Board.FindTurtle(TurtleId);
	return Turtle != nullptr && Turtle->Owner != PlayerId ? EGatherAndHuntTurtleRole::Opponent : EGatherAndHuntTurtleRole::Player;
}

bool UGatherAndHuntTurnPlanner::IsEditable(const turtlesim::Turtle& Turtle) const
{
	const bool bScripted = GetTurtleRole(Turtle.Id) != EGatherAndHuntTurtleRole::Player;
	return IsPlayable(Turtle) && bScripted == bEditingScript;
}

void UGatherAndHuntTurnPlanner::RebuildEditableTurtles()
{
	PlayerTurtleIds.Reset();
	for (const turtlesim::Turtle& Turtle : Board.Turtles)
	{
		if (IsEditable(Turtle))
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

void UGatherAndHuntTurnPlanner::LoadScriptDraftFromSession()
{
	for (auto It = MovesByTurtle.CreateIterator(); It; ++It)
	{
		if (GetTurtleRole(It.Key()) != EGatherAndHuntTurtleRole::Player)
		{
			It.RemoveCurrent();
		}
	}

	if (bHasBoard && Session != nullptr)
	{
		for (const FGatherAndHuntPuzzleTurtlePlan& TurtlePlan : Session->GetScriptedTurn(Board.TurnNumber).TurtlePlans)
		{
			const turtlesim::Turtle* const Turtle = Board.FindTurtle(TurtlePlan.TurtleId);
			if (Turtle != nullptr && IsPlayable(*Turtle) && TurtlePlan.Moves.Num() > 0
				&& GetTurtleRole(TurtlePlan.TurtleId) != EGatherAndHuntTurtleRole::Player)
			{
				MovesByTurtle.Add(TurtlePlan.TurtleId, TurtlePlan.Moves);
			}
		}
	}
	bScriptDraftDirty = false;
}

void UGatherAndHuntTurnPlanner::MarkEdited()
{
	if (bEditingScript)
	{
		bScriptDraftDirty = true;
	}
	Recompute();
}

void UGatherAndHuntTurnPlanner::SetEditingScript(const bool bEditScript)
{
	if (bEditingScript == bEditScript)
	{
		return;
	}
	bEditingScript = bEditScript;
	SelectedTurtleId = INDEX_NONE;
	SelectedMoveIndex = INDEX_NONE;
	RebuildEditableTurtles();
	Recompute();
}

bool UGatherAndHuntTurnPlanner::SaveScriptedTurn(FGatherAndHuntPuzzlePlanValidation& OutValidation)
{
	OutValidation = FGatherAndHuntPuzzlePlanValidation{};
	if (Session == nullptr || !bHasBoard)
	{
		OutValidation.bIsValid = false;
		OutValidation.ErrorMessage = TEXT("No puzzle loaded.");
		return false;
	}

	// Each team has its own budget, so validate the ally and opponent parts separately.
	const turtlesim::TurnPlan AllyDraft = ToSimPlan(BuildPlanForRoles(false, true, false), PlayerId);
	const turtlesim::TurnPlan OpponentDraft = ToSimPlan(BuildPlanForRoles(false, false, true), 1 - PlayerId);
	OutValidation = UGatherAndHuntPuzzleSession::BuildValidation(turtlesim::ValidateTurnPlan(Board, AllyDraft, Budget));
	if (OutValidation.bIsValid)
	{
		OutValidation = UGatherAndHuntPuzzleSession::BuildValidation(turtlesim::ValidateTurnPlan(Board, OpponentDraft, Budget));
	}
	if (!OutValidation.bIsValid || !Session->SetScriptedTurn(Board.TurnNumber, BuildPlanForRoles(false, true, true)))
	{
		return false;
	}

	bScriptDraftDirty = false;
	Recompute();
	return true;
}

void UGatherAndHuntTurnPlanner::RevertScriptDraft()
{
	LoadScriptDraftFromSession();
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
	// Budget and cost are per team, and the selected turtle decides the team in script mode.
	Recompute();
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
	if (SelectedTurtleId == INDEX_NONE || !IsMoveAvailable(MoveKind))
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

bool UGatherAndHuntTurnPlanner::IsMoveAvailable(const EGatherAndHuntPuzzleMoveKind MoveKind) const
{
	// The puzzle limits the player's moves; scripted turtles can use any.
	return bEditingScript || Session == nullptr || Session->IsMoveAllowed(MoveKind);
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
	MoveMove(SelectedMoveIndex, SelectedMoveIndex + FMath::Sign(Direction));
}

void UGatherAndHuntTurnPlanner::MoveMove(const int32 FromIndex, const int32 ToIndex)
{
	TArray<EGatherAndHuntPuzzleMoveKind>* const Moves = FindSelectedMoves();
	if (Moves == nullptr || !Moves->IsValidIndex(FromIndex) || !Moves->IsValidIndex(ToIndex) || FromIndex == ToIndex)
	{
		return;
	}
	const EGatherAndHuntPuzzleMoveKind Moved = (*Moves)[FromIndex];
	Moves->RemoveAt(FromIndex);
	Moves->Insert(Moved, ToIndex);
	SelectedMoveIndex = ToIndex;
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

FGatherAndHuntPuzzleTurnPlan UGatherAndHuntTurnPlanner::GetTurnPlan() const
{
	return bEditingScript ? BuildPlanForRoles(false, true, true) : BuildPlanForRoles(true, false, false);
}

FGatherAndHuntPuzzleTurnPlan UGatherAndHuntTurnPlanner::BuildPlanForRoles(const bool bPlayer, const bool bAlly, const bool bOpponent) const
{
	FGatherAndHuntPuzzleTurnPlan Plan;
	for (const turtlesim::Turtle& Turtle : Board.Turtles)
	{
		const TArray<EGatherAndHuntPuzzleMoveKind>* const Moves = MovesByTurtle.Find(Turtle.Id);
		if (!IsPlayable(Turtle) || Moves == nullptr || Moves->Num() == 0)
		{
			continue;
		}
		const EGatherAndHuntTurtleRole Role = GetTurtleRole(Turtle.Id);
		if ((Role == EGatherAndHuntTurtleRole::Player && bPlayer)
			|| (Role == EGatherAndHuntTurtleRole::Ally && bAlly)
			|| (Role == EGatherAndHuntTurtleRole::Opponent && bOpponent))
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

void UGatherAndHuntTurnPlanner::SetPreviewIncludesScripted(const bool bInclude)
{
	if (bPreviewIncludesScripted != bInclude)
	{
		bPreviewIncludesScripted = bInclude;
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

	const FGatherAndHuntPuzzleTurnPlan Plan = BuildPlanForRoles(true, false, false);
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

	for (const FGatherAndHuntPuzzleTurtlePlan& TurtlePlan : Plan.TurtlePlans)
	{
		MovesByTurtle.Remove(TurtlePlan.TurtleId);
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

	// Team plans for the preview. The player's own queue always counts. Scripted
	// turtles use the draft while authoring, or the puzzle's script otherwise.
	const int32 OpponentId = 1 - PlayerId;
	const turtlesim::TurnPlan PlayerPart = ToSimPlan(BuildPlanForRoles(true, false, false), PlayerId);
	turtlesim::TurnPlan AllyPart;
	turtlesim::TurnPlan OpponentPart;
	if (bEditingScript)
	{
		AllyPart = ToSimPlan(BuildPlanForRoles(false, true, false), PlayerId);
		OpponentPart = ToSimPlan(BuildPlanForRoles(false, false, true), OpponentId);
	}
	else if (bPreviewIncludesScripted && Session != nullptr)
	{
		Session->BuildScriptedPlansForCurrentTurn(AllyPart, OpponentPart);
	}

	// Validation and cost cover what's being edited, per team budget.
	if (bEditingScript)
	{
		const bool bOpponentSelected = GetTurtleRole(SelectedTurtleId) == EGatherAndHuntTurtleRole::Opponent;
		TotalCost = turtlesim::CostModel::TurnCost(bOpponentSelected ? OpponentPart : AllyPart);
		Validation = UGatherAndHuntPuzzleSession::BuildValidation(turtlesim::ValidateTurnPlan(Board, AllyPart, Budget));
		if (Validation.bIsValid)
		{
			Validation = UGatherAndHuntPuzzleSession::BuildValidation(turtlesim::ValidateTurnPlan(Board, OpponentPart, Budget));
		}
	}
	else
	{
		TotalCost = turtlesim::CostModel::TurnCost(PlayerPart);
		Validation = UGatherAndHuntPuzzleSession::BuildValidation(turtlesim::ValidateTurnPlan(Board, PlayerPart, Budget));
		if (Validation.bIsValid && Session != nullptr)
		{
			Validation = Session->CheckPuzzleRules(BuildPlanForRoles(true, false, false));
		}
	}

	turtlesim::TurnPlan TeamPlan = PlayerPart;
	AppendPlan(TeamPlan, AllyPart);
	OpponentPart.Player = OpponentId;

	// Preview even when invalid so the ghost path keeps tracking edits; the
	// resolver treats illegal pitch moves as no-ops, and the UI flags the error.
	const turtlesim::TurnResult Result = turtlesim::ResolveTurn(Board, TeamPlan, OpponentPart);

	PreviewSummary.ScoreBefore = PlayerId == 0 ? Board.ScorePlayer0 : Board.ScorePlayer1;
	PreviewSummary.ScoreAfter = PlayerId == 0 ? Result.FinalState.ScorePlayer0 : Result.FinalState.ScorePlayer1;
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
		Track.Role = GetTurtleRole(Turtle.Id);
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

		const EGatherAndHuntPlannerStepKind Fate = Track.Steps.Num() > 0 ? Track.Steps.Last().Kind : EGatherAndHuntPlannerStepKind::Start;
		int32 ClaimedPoints = 0;
		for (const FGatherAndHuntPlannerTrackStep& Step : Track.Steps)
		{
			ClaimedPoints += Step.bClaimedObjective ? Step.PointsDelta : 0;
		}

		// The summary counts the player's whole team (player turtles and allies).
		if (Turtle.Owner == PlayerId)
		{
			for (const FGatherAndHuntPlannerTrackStep& Step : Track.Steps)
			{
				PreviewSummary.ObjectivesClaimed += Step.bClaimedObjective ? 1 : 0;
			}
			PreviewSummary.TurtlesLost += Fate == EGatherAndHuntPlannerStepKind::Destroyed ? 1 : 0;
			PreviewSummary.TurtlesDelivered += Fate == EGatherAndHuntPlannerStepKind::Delivered ? 1 : 0;
		}

		if (!IsEditable(Turtle))
		{
			continue;
		}

		FGatherAndHuntPlannerTurtleSummary& Summary = TurtleSummaries.AddDefaulted_GetRef();
		Summary.TurtleId = Turtle.Id;
		Summary.Role = Track.Role;
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
