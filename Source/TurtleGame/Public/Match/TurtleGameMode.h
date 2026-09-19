#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "BoardState.h"
#include "Move.h"
#include "Match/MatchPhase.h"
#include "TurtleGameMode.generated.h"

class ATurtleGameState;
class ATurtlePlayerController;

UCLASS()
class TURTLEGAME_API ATurtleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATurtleGameMode();

	// --- Tunables (spec §17). Exposed via Blueprint defaults for level-by-level overrides. ---
	UPROPERTY(EditDefaultsOnly, Category = "Turtle|Match")
	int32 BoardSize = 11;

	UPROPERTY(EditDefaultsOnly, Category = "Turtle|Match")
	int32 TurtlesPerPlayer = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Turtle|Match")
	int32 StaticObstacles = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Turtle|Match")
	int32 Hunters = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Turtle|Match")
	int32 MoveBudget = 20;

	UPROPERTY(EditDefaultsOnly, Category = "Turtle|Match")
	int32 MaxTurns = 20;

	UPROPERTY(EditDefaultsOnly, Category = "Turtle|Match")
	int32 BoardSeed = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Turtle|Match")
	float PlanningTimeoutSeconds = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Turtle|Match")
	float PlaybackTimeoutSeconds = 2.0f;

	// --- AGameModeBase overrides ---
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// Called by ATurtlePlayerController on plan submission. Validates and stores;
	// kicks off resolution when both plans have arrived.
	void HandleSubmittedPlan(int32 PlayerSlot, const TArray<uint8>& SerializedPlan);

	// Called by ATurtlePlayerController when a client confirms it finished animating.
	void HandlePlaybackAck(int32 PlayerSlot, int32 TurnNumber);

	// Allows tests / console to bootstrap a match without going through PostLogin.
	UFUNCTION(BlueprintCallable, Category = "Turtle|Match")
	void StartMatchWithGeneratedBoard();

protected:
	// Server-only authoritative simulation state.
	turtlesim::BoardState ServerBoardState;
	turtlesim::TurnPlan   PendingPlan0;
	turtlesim::TurnPlan   PendingPlan1;
	bool bHasPlan0 = false;
	bool bHasPlan1 = false;

	// Track which clients have ack'd the current turn's playback.
	int32 LastResolvedTurn = -1;
	bool bPlayer0AckedPlayback = false;
	bool bPlayer1AckedPlayback = false;
	TArray<uint8> LastTurnResultBytes;
	FTimerHandle PlanningTimeoutHandle;
	FTimerHandle PlaybackTimeoutHandle;

	// Slot assignment.
	TWeakObjectPtr<ATurtlePlayerController> SlotControllers[2];

	void TransitionTo(EMatchPhase NewPhase);
	void BroadcastReplicatedState();
	void ConfigurePhaseTimers(EMatchPhase NewPhase);
	void HandlePlanningTimeout();
	void HandlePlaybackTimeout();
	void SendCurrentStateTo(ATurtlePlayerController* PC);
	void TryAdvanceFromAwaitingPlans();
	void ResolveAndBroadcast();
	void CheckEndConditions();
	void StartNextPlanningPhaseIfReady();

	ATurtleGameState* GetTurtleGameState() const;
	int32 AssignSlotForNewPlayer(ATurtlePlayerController* PC);
	void ReleaseSlot(int32 Slot);
};
