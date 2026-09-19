#include "Match/TurtlePlayerController.h"

#include "Net/UnrealNetwork.h"
#include "Match/TurtleGameMode.h"
#include "Match/TurtleGameState.h"
#include "Net/TurtleSerializer.h"

void ATurtlePlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATurtlePlayerController, SlotIndex);
}

void ATurtlePlayerController::ServerSubmitTurnPlan_Implementation(const TArray<uint8>& SerializedPlan)
{
	if (SlotIndex != 0 && SlotIndex != 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerSubmitTurnPlan: rejecting plan from non-player slot %d"), SlotIndex);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (ATurtleGameMode* GM = World->GetAuthGameMode<ATurtleGameMode>())
		{
			GM->HandleSubmittedPlan(SlotIndex, SerializedPlan);
		}
	}
}

void ATurtlePlayerController::ClientReceiveTickLog_Implementation(const TArray<uint8>& SerializedTurnResult)
{
	// Phase 3 will hand this to the playback controller. For now, log so a
	// headless test can confirm round-trip delivery.
	UE_LOG(LogTemp, Log, TEXT("ClientReceiveTickLog: received %d bytes"), SerializedTurnResult.Num());

	turtlesim::TurnResult Decoded;
	if (TurtleSimWire::DeserializeTurnResult(SerializedTurnResult, Decoded))
	{
		if (ATurtleGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATurtleGameState>() : nullptr)
		{
			GS->ApplyTurnResult(Decoded);
		}
		ServerAcknowledgePlayback(Decoded.FinalState.TurnNumber);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ClientReceiveTickLog: failed to deserialize turn result"));
	}
}

void ATurtlePlayerController::ClientReceiveFullState_Implementation(const TArray<uint8>& SerializedBoardState)
{
	UE_LOG(LogTemp, Log, TEXT("ClientReceiveFullState: %d bytes"), SerializedBoardState.Num());

	turtlesim::BoardState Decoded;
	if (TurtleSimWire::DeserializeBoardState(SerializedBoardState, Decoded))
	{
		if (ATurtleGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATurtleGameState>() : nullptr)
		{
			GS->ApplyFullBoardState(Decoded);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ClientReceiveFullState: failed to deserialize board state"));
	}
}

void ATurtlePlayerController::ServerAcknowledgePlayback_Implementation(int32 TurnNumber)
{
	if (UWorld* World = GetWorld())
	{
		if (ATurtleGameMode* GM = World->GetAuthGameMode<ATurtleGameMode>())
		{
			GM->HandlePlaybackAck(SlotIndex, TurnNumber);
		}
	}
}
