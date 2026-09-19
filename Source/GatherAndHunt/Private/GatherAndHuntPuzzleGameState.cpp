#include "GatherAndHuntPuzzleGameState.h"

#include "GatherAndHuntPuzzleSession.h"

void AGatherAndHuntPuzzleGameState::ApplySessionState(const UGatherAndHuntPuzzleSession* PuzzleSession)
{
	if (PuzzleSession == nullptr || !PuzzleSession->IsPuzzleLoaded())
	{
		bPuzzleLoaded = false;
		PuzzleName.Reset();
		TurnNumber = 0;
		PlayerScore = 0;
		TargetScore = 0;
		TargetTurnCount = 0;
		TargetObjectiveCount = 0;
		Outcome = EGatherAndHuntPuzzleOutcome::NotLoaded;
		return;
	}

	bPuzzleLoaded = true;
	PuzzleName = PuzzleSession->GetLoadedPuzzleName();
	TurnNumber = PuzzleSession->GetCurrentTurnNumber();
	PlayerScore = PuzzleSession->GetPlayerScore();
	TargetScore = PuzzleSession->GetTargetScore();
	TargetTurnCount = PuzzleSession->GetTargetTurnCount();
	TargetObjectiveCount = PuzzleSession->GetTargetObjectiveCount();
	Outcome = PuzzleSession->GetOutcome();
}
