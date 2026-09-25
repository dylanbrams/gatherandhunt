#include "Match/TurtleGameMode.h"

#include "Match/TurtleGameState.h"
#include "Match/TurtlePlayerController.h"
#include "Net/TurtleSerializer.h"

#include "BoardGenerator.h"
#include "Resolver.h"
#include "Validation.h"
#include "Constants.h"

ATurtleGameMode::ATurtleGameMode()
{
	GameStateClass        = ATurtleGameState::StaticClass();
	PlayerControllerClass = ATurtlePlayerController::StaticClass();
}

void ATurtleGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		// Match begins in WaitingForPlayers; PostLogin will kick off Planning when full.
		if (ATurtleGameState* GS = GetTurtleGameState())
		{
			GS->MatchState.Phase = EMatchPhase::WaitingForPlayers;
			BroadcastReplicatedState();
		}
	}
}

ATurtleGameState* ATurtleGameMode::GetTurtleGameState() const
{
	return GetGameState<ATurtleGameState>();
}

int32 ATurtleGameMode::AssignSlotForNewPlayer(ATurtlePlayerController* PC)
{
	for (int32 i = 0; i < 2; ++i)
	{
		if (!SlotControllers[i].IsValid())
		{
			SlotControllers[i] = PC;
			PC->SlotIndex = i;
			return i;
		}
	}
	PC->SlotIndex = -1; // spectator.
	return -1;
}

void ATurtleGameMode::ReleaseSlot(int32 Slot)
{
	if (Slot >= 0 && Slot < 2) SlotControllers[Slot] = nullptr;
}

void ATurtleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ATurtlePlayerController* PC = Cast<ATurtlePlayerController>(NewPlayer);
	if (!PC) return;

	const int32 Slot = AssignSlotForNewPlayer(PC);
	UE_LOG(LogTemp, Log, TEXT("TurtleGameMode: assigned slot %d to %s"), Slot, *PC->GetName());

	const ATurtleGameState* GS = GetTurtleGameState();
	const bool bMatchAlreadyStarted = GS && GS->MatchState.Phase != EMatchPhase::WaitingForPlayers;

	if (bMatchAlreadyStarted)
	{
		SendCurrentStateTo(PC);
	}

	// Once both slots are filled, start the match.
	if (SlotControllers[0].IsValid() && SlotControllers[1].IsValid() && !bMatchAlreadyStarted)
	{
		StartMatchWithGeneratedBoard();
	}
}

void ATurtleGameMode::Logout(AController* Exiting)
{
	if (ATurtlePlayerController* PC = Cast<ATurtlePlayerController>(Exiting))
	{
		ReleaseSlot(PC->SlotIndex);
	}
	Super::Logout(Exiting);
}

void ATurtleGameMode::StartMatchWithGeneratedBoard()
{
	turtlesim::BoardGenParams Params;
	Params.Size              = turtlesim::IntVec3(BoardSize, BoardSize, BoardSize);
	Params.TurtlesPerPlayer  = TurtlesPerPlayer;
	Params.StaticObstacles   = StaticObstacles;
	Params.Hunters           = Hunters;
	Params.ObjectivePointPool = { 1, 1, 1, 3, 3, 5 }; // mirrored => 12 objectives.
	Params.Seed              = static_cast<uint32>(BoardSeed != 0 ? BoardSeed : FMath::Rand());

	turtlesim::BoardGenResult Gen = turtlesim::GenerateBoard(Params);
	if (!Gen.bOk)
	{
		UE_LOG(LogTemp, Error, TEXT("BoardGenerator failed after %d retries"), Gen.Retries);
		return;
	}

	ServerBoardState = std::move(Gen.State);
	PendingPlan0 = turtlesim::TurnPlan{};
	PendingPlan1 = turtlesim::TurnPlan{};
	bHasPlan0 = bHasPlan1 = false;
	LastResolvedTurn = -1;
	LastTurnResultBytes.Reset();
	bMatchEndPending = false;
	PendingWinnerSlot = -1;
	PendingEndReason = EMatchEndReason::NotEnded;

	// Send everyone the initial board.
	if (ATurtleGameState* GS = GetTurtleGameState())
	{
		TurtleSimWire::SerializeBoardState(GS->MatchState.SerializedBoardState, ServerBoardState);
		GS->MatchState.TurnNumber       = ServerBoardState.TurnNumber;
		GS->MatchState.ScorePlayer0     = ServerBoardState.ScorePlayer0;
		GS->MatchState.ScorePlayer1     = ServerBoardState.ScorePlayer1;
		GS->MatchState.bPlayer0Submitted = false;
		GS->MatchState.bPlayer1Submitted = false;
		GS->MatchState.WinnerSlot        = -1;
		GS->MatchState.EndReason         = EMatchEndReason::NotEnded;
	}
	TransitionTo(EMatchPhase::Planning);
}

void ATurtleGameMode::TransitionTo(EMatchPhase NewPhase)
{
	ConfigurePhaseTimers(NewPhase);
	if (ATurtleGameState* GS = GetTurtleGameState())
	{
		GS->MatchState.Phase = NewPhase;
		BroadcastReplicatedState();
	}
	UE_LOG(LogTemp, Log, TEXT("TurtleGameMode: -> %s"),
		*UEnum::GetValueAsString(NewPhase));
}

void ATurtleGameMode::ConfigurePhaseTimers(EMatchPhase NewPhase)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlanningTimeoutHandle);
		World->GetTimerManager().ClearTimer(PlaybackTimeoutHandle);

		if ((NewPhase == EMatchPhase::Planning || NewPhase == EMatchPhase::AwaitingPlans) && PlanningTimeoutSeconds > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				PlanningTimeoutHandle,
				this,
				&ATurtleGameMode::HandlePlanningTimeout,
				PlanningTimeoutSeconds,
				false);
		}
		else if (NewPhase == EMatchPhase::Animating && PlaybackTimeoutSeconds > 0.0f)
		{
			// Clients acknowledge when their animation finishes, so the deadline has
			// to clear the animation itself or every long turn would time out.
			const float Timeout = PlaybackTimeoutSeconds
				+ LastTurnTickCount * FMath::Max(0.0f, EstimatedSecondsPerTick);
			World->GetTimerManager().SetTimer(
				PlaybackTimeoutHandle,
				this,
				&ATurtleGameMode::HandlePlaybackTimeout,
				Timeout,
				false);
		}
	}
}

void ATurtleGameMode::HandlePlanningTimeout()
{
	if (ATurtleGameState* GS = GetTurtleGameState())
	{
		if (GS->MatchState.Phase != EMatchPhase::Planning && GS->MatchState.Phase != EMatchPhase::AwaitingPlans)
		{
			return;
		}
	}

	if (!bHasPlan0)
	{
		PendingPlan0 = turtlesim::TurnPlan{};
		PendingPlan0.Player = 0;
		bHasPlan0 = true;
	}
	if (!bHasPlan1)
	{
		PendingPlan1 = turtlesim::TurnPlan{};
		PendingPlan1.Player = 1;
		bHasPlan1 = true;
	}

	if (ATurtleGameState* GS = GetTurtleGameState())
	{
		GS->MatchState.bPlayer0Submitted = bHasPlan0;
		GS->MatchState.bPlayer1Submitted = bHasPlan1;
		BroadcastReplicatedState();
	}

	ResolveAndBroadcast();
}

void ATurtleGameMode::HandlePlaybackTimeout()
{
	if (ATurtleGameState* GS = GetTurtleGameState())
	{
		if (GS->MatchState.Phase != EMatchPhase::Animating)
		{
			return;
		}
	}

	bPlayer0AckedPlayback = true;
	bPlayer1AckedPlayback = true;
	StartNextPlanningPhaseIfReady();
}

void ATurtleGameMode::SendCurrentStateTo(ATurtlePlayerController* PC)
{
	if (!PC) return;

	if (ATurtleGameState* GS = GetTurtleGameState())
	{
		if (GS->MatchState.SerializedBoardState.Num() > 0)
		{
			PC->ClientReceiveFullState(GS->MatchState.SerializedBoardState);
		}
		if (GS->MatchState.Phase == EMatchPhase::Animating && LastTurnResultBytes.Num() > 0)
		{
			PC->ClientReceiveTickLog(LastTurnResultBytes);
		}
	}
}

void ATurtleGameMode::BroadcastReplicatedState()
{
	// Replication is automatic via DOREPLIFETIME on ATurtleGameState::MatchState;
	// touching the field with ForceNetUpdate ensures clients see it promptly.
	if (ATurtleGameState* GS = GetTurtleGameState())
	{
		GS->ForceNetUpdate();
	}
}

void ATurtleGameMode::HandleSubmittedPlan(int32 PlayerSlot, const TArray<uint8>& SerializedPlan)
{
	if (PlayerSlot != 0 && PlayerSlot != 1) return;

	turtlesim::TurnPlan Decoded;
	if (!TurtleSimWire::DeserializeTurnPlan(SerializedPlan, Decoded))
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleSubmittedPlan: malformed plan from slot %d"), PlayerSlot);
		return;
	}
	// Force the player id to match the slot — the client should set it but we don't trust it.
	Decoded.Player = PlayerSlot;

	const auto V = turtlesim::ValidateTurnPlan(ServerBoardState, Decoded, MoveBudget);
	if (V.Error != turtlesim::EPlanValidationError::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleSubmittedPlan: invalid plan slot=%d err=%d turtle=%d move=%d"),
			PlayerSlot, static_cast<int32>(V.Error), V.OffendingTurtle, V.OffendingMoveIndex);
		return;
	}

	if (PlayerSlot == 0) { PendingPlan0 = std::move(Decoded); bHasPlan0 = true; }
	else                 { PendingPlan1 = std::move(Decoded); bHasPlan1 = true; }

	if (ATurtleGameState* GS = GetTurtleGameState())
	{
		GS->MatchState.bPlayer0Submitted = bHasPlan0;
		GS->MatchState.bPlayer1Submitted = bHasPlan1;
		BroadcastReplicatedState();
	}

	if (bHasPlan0 && bHasPlan1)
	{
		ResolveAndBroadcast();
	}
	else
	{
		TransitionTo(EMatchPhase::AwaitingPlans);
	}
}

void ATurtleGameMode::ResolveAndBroadcast()
{
	TransitionTo(EMatchPhase::Resolving);

	turtlesim::TurnResult Result =
		turtlesim::ResolveTurn(ServerBoardState, PendingPlan0, PendingPlan1);
	ServerBoardState = Result.FinalState;

	// Clear pending plans for the next turn.
	bHasPlan0 = bHasPlan1 = false;
	PendingPlan0 = turtlesim::TurnPlan{};
	PendingPlan1 = turtlesim::TurnPlan{};
	bPlayer0AckedPlayback = bPlayer1AckedPlayback = false;
	LastResolvedTurn = ServerBoardState.TurnNumber;
	LastTurnTickCount = static_cast<int32>(Result.Ticks.size());

	// Refresh replicated state.
	if (ATurtleGameState* GS = GetTurtleGameState())
	{
		TurtleSimWire::SerializeBoardState(GS->MatchState.SerializedBoardState, ServerBoardState);
		GS->MatchState.TurnNumber        = ServerBoardState.TurnNumber;
		GS->MatchState.ScorePlayer0      = ServerBoardState.ScorePlayer0;
		GS->MatchState.ScorePlayer1      = ServerBoardState.ScorePlayer1;
		GS->MatchState.bPlayer0Submitted = false;
		GS->MatchState.bPlayer1Submitted = false;
	}

	// Send the tick log to every client.
	TArray<uint8> Bytes;
	TurtleSimWire::SerializeTurnResult(Bytes, Result);
	LastTurnResultBytes = Bytes;
	for (int32 i = 0; i < 2; ++i)
	{
		if (SlotControllers[i].IsValid())
		{
			SlotControllers[i]->ClientReceiveTickLog(Bytes);
		}
	}

	TransitionTo(EMatchPhase::Animating);
	CheckEndConditions();
}

void ATurtleGameMode::HandlePlaybackAck(int32 PlayerSlot, int32 TurnNumber)
{
	if (TurnNumber != LastResolvedTurn) return;
	if (PlayerSlot == 0) bPlayer0AckedPlayback = true;
	if (PlayerSlot == 1) bPlayer1AckedPlayback = true;
	StartNextPlanningPhaseIfReady();
}

void ATurtleGameMode::StartNextPlanningPhaseIfReady()
{
	if (!(bPlayer0AckedPlayback && bPlayer1AckedPlayback)) return;
	if (GetTurtleGameState() && GetTurtleGameState()->MatchState.Phase == EMatchPhase::Ended) return;

	if (bMatchEndPending)
	{
		if (ATurtleGameState* GS = GetTurtleGameState())
		{
			GS->MatchState.WinnerSlot = PendingWinnerSlot;
			GS->MatchState.EndReason  = PendingEndReason;
		}
		TransitionTo(EMatchPhase::Ended);
		return;
	}

	TransitionTo(EMatchPhase::Planning);
}

FTurtleMatchOutcome ATurtleGameMode::EvaluateMatchOutcome(const turtlesim::BoardState& State, int32 InMaxTurns)
{
	bool bAllObjectivesDone = true;
	for (const auto& O : State.Objectives)
	{
		if (!O.bClaimed) { bAllObjectivesDone = false; break; }
	}
	// A delivered turtle is still alive, it just isn't on the board any more, so
	// "in play" and "surviving" are different counts and the tiebreaks need both.
	int32 InPlay0 = 0, InPlay1 = 0;
	int32 Delivered0 = 0, Delivered1 = 0;
	int32 Surviving0 = 0, Surviving1 = 0;
	bool bAnyCarrierStillActive = false;
	for (const auto& T : State.Turtles)
	{
		if (T.Owner == 0)
		{
			if (T.bAlive) ++Surviving0;
			if (T.bDelivered) ++Delivered0;
		}
		else if (T.Owner == 1)
		{
			if (T.bAlive) ++Surviving1;
			if (T.bDelivered) ++Delivered1;
		}

		if (!T.bAlive || T.bDelivered) continue;
		if (T.Owner == 0) ++InPlay0;
		if (T.Owner == 1) ++InPlay1;
		if (T.CarriedPoints > 0) bAnyCarrierStillActive = true;
	}

	const bool bTurnLimit = State.TurnNumber >= InMaxTurns;
	const bool bOutOfPlay = (InPlay0 == 0) || (InPlay1 == 0);
	const bool bAllDone   = bAllObjectivesDone && !bAnyCarrierStillActive;

	FTurtleMatchOutcome Outcome;
	if (!(bTurnLimit || bOutOfPlay || bAllDone))
	{
		return Outcome;
	}

	Outcome.bEnded = true;
	// Several conditions can fire on the same turn, so one has to be picked; the
	// order runs most specific to least.
	Outcome.Reason = bOutOfPlay ? EMatchEndReason::TeamOutOfPlay
		: (bAllDone ? EMatchEndReason::ObjectivesGone : EMatchEndReason::TurnLimit);

	// Spec §13: score, then turtles delivered, then turtles surviving, then a draw.
	// Points only count once delivered, so a side wiped out while ahead still wins
	// on what it banked.
	const int32 Ladder[][2] = {
		{ State.ScorePlayer0, State.ScorePlayer1 },
		{ Delivered0,         Delivered1         },
		{ Surviving0,         Surviving1         },
	};
	Outcome.WinnerSlot = -1;
	for (const int32(&Rung)[2] : Ladder)
	{
		if (Rung[0] != Rung[1])
		{
			Outcome.WinnerSlot = Rung[0] > Rung[1] ? 0 : 1;
			break;
		}
	}
	return Outcome;
}

void ATurtleGameMode::CheckEndConditions()
{
	const FTurtleMatchOutcome Outcome = EvaluateMatchOutcome(ServerBoardState, MaxTurns);
	if (!Outcome.bEnded)
	{
		return;
	}

	// The end is recorded, not applied: clients are animating the turn that
	// triggered it, and switching to Ended now would cut that animation short.
	// StartNextPlanningPhaseIfReady enters Ended once playback is acknowledged.
	bMatchEndPending  = true;
	PendingEndReason  = Outcome.Reason;
	PendingWinnerSlot = Outcome.WinnerSlot;

	UE_LOG(LogTemp, Log, TEXT("TurtleGameMode: match end pending, reason=%s winner=%d (%d-%d)"),
		*UEnum::GetValueAsString(Outcome.Reason), Outcome.WinnerSlot,
		ServerBoardState.ScorePlayer0, ServerBoardState.ScorePlayer1);
}
