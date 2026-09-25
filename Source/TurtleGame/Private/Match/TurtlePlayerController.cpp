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
	UE_LOG(LogTemp, Log, TEXT("ClientReceiveTickLog: received %d bytes"), SerializedTurnResult.Num());

	turtlesim::TurnResult Decoded;
	if (TurtleSimWire::DeserializeTurnResult(SerializedTurnResult, Decoded))
	{
		ATurtleGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATurtleGameState>() : nullptr;
		if (GS)
		{
			GS->ApplyTurnResult(Decoded);
		}
		// A presenter that animates the turn acknowledges it when the animation
		// finishes; acknowledging here as well would let the server start the next
		// turn while this client is still playing the last one back. With no
		// presenter there is nothing to wait for, so acknowledge straight away
		// rather than making the server wait out its playback timeout.
		if (!GS || !GS->HasPlaybackObserver())
		{
			ServerAcknowledgePlayback(Decoded.FinalState.TurnNumber);
		}
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
