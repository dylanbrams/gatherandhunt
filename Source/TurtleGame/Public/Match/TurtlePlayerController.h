#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TurtlePlayerController.generated.h"

UCLASS()
class TURTLEGAME_API ATurtlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Server-assigned player slot. 0 or 1 for active players; -1 for spectators.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Turtle|Match")
	int32 SlotIndex = -1;

	// Client → Server. The serialized payload is a turtlesim::TurnPlan; see TurtleSerializer.
	UFUNCTION(Server, Reliable)
	void ServerSubmitTurnPlan(const TArray<uint8>& SerializedPlan);

	// Server → Client. Delivered after both plans are resolved; clients walk the
	// tick log to drive playback (Phase 3).
	UFUNCTION(Client, Reliable)
	void ClientReceiveTickLog(const TArray<uint8>& SerializedTurnResult);

	// Server → Client. Used on join / reconnect / desync recovery.
	UFUNCTION(Client, Reliable)
	void ClientReceiveFullState(const TArray<uint8>& SerializedBoardState);

	// Optional acknowledgement so the server can advance Animating → Planning
	// when both clients confirm playback is done. (Phase 3 wires this fully.)
	UFUNCTION(Server, Reliable)
	void ServerAcknowledgePlayback(int32 TurnNumber);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
