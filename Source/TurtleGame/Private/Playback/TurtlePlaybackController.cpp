#include "Playback/TurtlePlaybackController.h"

#include "Playback/TurtleActor.h"
#include "Playback/ObstacleActor.h"
#include "Playback/ObjectiveActor.h"
#include "Match/TurtlePlayerController.h"

#include "GameFramework/Actor.h"

UTurtlePlaybackController::UTurtlePlaybackController()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UTurtlePlaybackController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bIsPlaying || ActiveTickIndex == INDEX_NONE) return;

	TickElapsed += DeltaTime;
	if (TickElapsed >= FMath::Max(0.001f, SecondsPerTick))
	{
		FinishCurrentTick();
	}
}

void UTurtlePlaybackController::RegisterTurtle(int32 TurtleId, ATurtleActor* Actor)
{
	if (Actor) { Actor->TurtleId = TurtleId; Turtles.Add(TurtleId, Actor); }
}
void UTurtlePlaybackController::RegisterObstacle(int32 ObstacleId, AObstacleActor* Actor)
{
	if (Actor) { Actor->ObstacleId = ObstacleId; Obstacles.Add(ObstacleId, Actor); }
}
void UTurtlePlaybackController::RegisterObjective(int32 ObjectiveId, AObjectiveActor* Actor)
{
	if (Actor) { Actor->ObjectiveId = ObjectiveId; Objectives.Add(ObjectiveId, Actor); }
}
void UTurtlePlaybackController::ClearRegistered()
{
	Turtles.Reset(); Obstacles.Reset(); Objectives.Reset();
}

FVector UTurtlePlaybackController::GridToWorld(const turtlesim::IntVec3& G) const
{
	return FVector(G.X * GridCellSize, G.Y * GridCellSize, G.Z * GridCellSize);
}

FRotator UTurtlePlaybackController::OrientationToWorldRotation(const turtlesim::Orientation& O) const
{
	// Yaw from heading (NESW). Pitch handled with simple +/- pitch axis.
	float Yaw = 0.f;
	switch (O.Heading)
	{
	case turtlesim::EHeading::North: Yaw = 90.f;  break; // +Y
	case turtlesim::EHeading::East:  Yaw = 0.f;   break; // +X
	case turtlesim::EHeading::South: Yaw = 270.f; break; // -Y
	case turtlesim::EHeading::West:  Yaw = 180.f; break; // -X
	}
	float Pitch = 0.f;
	if (O.Pitch == turtlesim::EPitch::Up)   Pitch = 90.f;
	if (O.Pitch == turtlesim::EPitch::Down) Pitch = -90.f;
	return FRotator(Pitch, Yaw, 0.f);
}

void UTurtlePlaybackController::ApplyBoardStateInstant(const turtlesim::BoardState& State)
{
	for (const auto& T : State.Turtles)
	{
		auto* Found = Turtles.Find(T.Id);
		if (!Found || !Found->IsValid()) continue;
		ATurtleActor* A = Found->Get();
		const FVector L = GridToWorld(T.Position);
		const FRotator R = OrientationToWorldRotation(T.Facing);
		A->SnapToPose(L, R);
		A->SetVisualVisible(T.bAlive && !T.bDelivered);
	}
	for (const auto& O : State.Obstacles)
	{
		auto* Found = Obstacles.Find(O.Id);
		if (!Found || !Found->IsValid()) continue;
		AObstacleActor* A = Found->Get();
		A->Kind = static_cast<int32>(O.Kind);
		A->SnapToLocation(GridToWorld(O.Position));
		A->SetVisualVisible(O.bAlive);
	}
	for (const auto& J : State.Objectives)
	{
		auto* Found = Objectives.Find(J.Id);
		if (!Found || !Found->IsValid()) continue;
		AObjectiveActor* A = Found->Get();
		A->PointValue = J.PointValue;
		A->SnapToLocation(GridToWorld(J.Position));
		A->SetVisualVisible(!J.bClaimed);
	}
}

void UTurtlePlaybackController::StartPlayback(const turtlesim::BoardState& StartState, const turtlesim::TurnResult& Result)
{
	if (Result.Ticks.empty())
	{
		SkipPlayback(Result);
		return;
	}

	ActiveStartState = StartState;
	ActiveResult     = Result;
	ApplyBoardStateInstant(StartState);
	bIsPlaying = true;
	PendingAckTurnNumber = Result.FinalState.TurnNumber;
	OnPlaybackStarted.Broadcast();
	BeginTick(0);
}

void UTurtlePlaybackController::SkipPlayback(const turtlesim::TurnResult& Result)
{
	bIsPlaying = false;
	ActiveTickIndex = INDEX_NONE;
	ActiveResult = Result;
	PendingAckTurnNumber = Result.FinalState.TurnNumber;
	// Snaps to ActiveResult.FinalState and acknowledges the turn.
	FinishPlayback();
}

void UTurtlePlaybackController::StopPlayback()
{
	if (bIsPlaying)
	{
		ApplyBoardStateInstant(ActiveResult.FinalState);
	}
	bIsPlaying = false;
	ActiveTickIndex = INDEX_NONE;
}

void UTurtlePlaybackController::BeginTick(int32 TickIndex)
{
	ActiveTickIndex = TickIndex;
	TickElapsed = 0.f;

	const auto& TR = ActiveResult.Ticks[static_cast<std::size_t>(TickIndex)];
	for (const auto& E : TR.Events)
	{
		switch (E.Type)
		{
		case turtlesim::EEventType::TurtleMoved:
		case turtlesim::EEventType::TurtleTurned:
		{
			auto* Found = Turtles.Find(E.PrimaryId);
			if (Found && Found->IsValid())
			{
				Found->Get()->SetTargetPose(GridToWorld(E.ToPosition), OrientationToWorldRotation(E.ToOrientation));
			}
			break;
		}
		case turtlesim::EEventType::ObstacleMoved:
		{
			auto* Found = Obstacles.Find(E.PrimaryId);
			if (Found && Found->IsValid())
			{
				Found->Get()->SetTargetLocation(GridToWorld(E.ToPosition));
			}
			break;
		}
		case turtlesim::EEventType::TurtleDestroyed:
		case turtlesim::EEventType::TurtleDelivered:
		{
			// Visual hide handled at end of tick (mirrors SP playback's defer).
			break;
		}
		case turtlesim::EEventType::ObstacleDestroyed:
			break;
		case turtlesim::EEventType::TurtleClaimedObjective:
		{
			auto* Found = Objectives.Find(E.SecondaryId);
			if (Found && Found->IsValid())
			{
				Found->Get()->SetVisualVisible(false);
			}
			break;
		}
		default:
			break;
		}
	}
	OnPlaybackTickStarted.Broadcast(TickIndex);
}

void UTurtlePlaybackController::FinishCurrentTick()
{
	const auto& TR = ActiveResult.Ticks[static_cast<std::size_t>(ActiveTickIndex)];
	for (const auto& E : TR.Events)
	{
		if (E.Type == turtlesim::EEventType::TurtleDestroyed
		 || E.Type == turtlesim::EEventType::TurtleDelivered)
		{
			auto* Found = Turtles.Find(E.PrimaryId);
			if (Found && Found->IsValid()) Found->Get()->SetVisualVisible(false);
		}
		else if (E.Type == turtlesim::EEventType::ObstacleDestroyed)
		{
			auto* Found = Obstacles.Find(E.PrimaryId);
			if (Found && Found->IsValid()) Found->Get()->SetVisualVisible(false);
		}
	}
	OnPlaybackTickFinished.Broadcast(ActiveTickIndex);

	const int32 NextIndex = ActiveTickIndex + 1;
	if (NextIndex >= static_cast<int32>(ActiveResult.Ticks.size()))
	{
		FinishPlayback();
	}
	else
	{
		BeginTick(NextIndex);
	}
}

void UTurtlePlaybackController::FinishPlayback()
{
	bIsPlaying = false;
	ActiveTickIndex = INDEX_NONE;
	ApplyBoardStateInstant(ActiveResult.FinalState);
	OnPlaybackFinished.Broadcast();

	if (bAutoAcknowledgeServer && PendingAckTurnNumber != INDEX_NONE)
	{
		// Find the local PlayerController and ack.
		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				ATurtlePlayerController* PC = Cast<ATurtlePlayerController>(It->Get());
				if (PC && PC->IsLocalController())
				{
					PC->ServerAcknowledgePlayback(PendingAckTurnNumber);
					break;
				}
			}
		}
	}
	PendingAckTurnNumber = INDEX_NONE;
}
