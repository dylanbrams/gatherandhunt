#include "GatherAndHuntPuzzleGameMode.h"

#include "GatherAndHuntPuzzleGameState.h"
#include "GatherAndHuntPuzzleHUD.h"
#include "GatherAndHuntPuzzleSession.h"
#include "Kismet/GameplayStatics.h"

AGatherAndHuntPuzzleGameMode::AGatherAndHuntPuzzleGameMode()
{
	GameStateClass = AGatherAndHuntPuzzleGameState::StaticClass();
	HUDClass = AGatherAndHuntPuzzleHUD::StaticClass();
	// Puzzle play is UI + board camera; no avatar to control.
	DefaultPawnClass = nullptr;
}

void AGatherAndHuntPuzzleGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	const FString PuzzleOption = UGameplayStatics::ParseOption(Options, TEXT("Puzzle"));
	if (!PuzzleOption.IsEmpty())
	{
		AutoLoadPuzzleRelativeFilename = PuzzleOption;
	}
	if (UGameplayStatics::HasOption(Options, TEXT("Edit")))
	{
		bStartInBoardEditor = true;
	}
}

void AGatherAndHuntPuzzleGameMode::BeginPlay()
{
	Super::BeginPlay();

	PuzzleSession = NewObject<UGatherAndHuntPuzzleSession>(this);
	RefreshGameStateFromSession();

	if (bAutoLoadPuzzleOnBeginPlay && !AutoLoadPuzzleRelativeFilename.IsEmpty())
	{
		LoadPuzzleFromRelativeFilename(AutoLoadPuzzleRelativeFilename);
	}
}

bool AGatherAndHuntPuzzleGameMode::LoadPuzzleFromRelativeFilename(const FString& RelativeFilename)
{
	if (PuzzleSession == nullptr)
	{
		PuzzleSession = NewObject<UGatherAndHuntPuzzleSession>(this);
	}

	const bool bLoaded = PuzzleSession->LoadPuzzleFromRelativeFilename(RelativeFilename);
	RefreshGameStateFromSession();
	return bLoaded;
}

bool AGatherAndHuntPuzzleGameMode::SubmitSoloTurnPlan(
	const FGatherAndHuntPuzzleTurnPlan& TurnPlan,
	FGatherAndHuntPuzzlePlanValidation& OutValidation)
{
	if (PuzzleSession == nullptr)
	{
		OutValidation = FGatherAndHuntPuzzlePlanValidation{};
		OutValidation.bIsValid = false;
		OutValidation.ErrorMessage = TEXT("Puzzle session is not available.");
		return false;
	}

	const bool bAccepted = PuzzleSession->SubmitSoloTurnPlan(TurnPlan, OutValidation);
	RefreshGameStateFromSession();
	return bAccepted;
}

bool AGatherAndHuntPuzzleGameMode::ResetPuzzle()
{
	if (PuzzleSession == nullptr)
	{
		return false;
	}

	const bool bReset = PuzzleSession->ResetToInitialState();
	RefreshGameStateFromSession();
	return bReset;
}

void AGatherAndHuntPuzzleGameMode::RefreshGameStateFromSession()
{
	if (AGatherAndHuntPuzzleGameState* PuzzleGameState = GetPuzzleGameState())
	{
		PuzzleGameState->ApplySessionState(PuzzleSession);
	}
}

AGatherAndHuntPuzzleGameState* AGatherAndHuntPuzzleGameMode::GetPuzzleGameState() const
{
	return GetGameState<AGatherAndHuntPuzzleGameState>();
}
