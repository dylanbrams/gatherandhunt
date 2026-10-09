#include "GatherAndHuntPuzzleGameMode.h"

#include "GatherAndHuntCampaign.h"
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
	CampaignNodeId = UGameplayStatics::ParseOption(Options, TEXT("CampaignNode"));
	if (!CampaignNodeId.IsEmpty())
	{
		FString Error;
		const UGatherAndHuntCampaign* const Campaign = UGatherAndHuntCampaign::Load(this, Error);
		bAuthoringToolsAvailable = Campaign != nullptr && Campaign->IsNodeCompleted(0);
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

bool AGatherAndHuntPuzzleGameMode::LoadPuzzleFromLevelSave(ULevelSave* LevelSave)
{
	if (PuzzleSession == nullptr)
	{
		PuzzleSession = NewObject<UGatherAndHuntPuzzleSession>(this);
	}

	const bool bLoaded = PuzzleSession->LoadPuzzleFromLevelSave(LevelSave);
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
	if (bAccepted && PuzzleSession->GetOutcome() == EGatherAndHuntPuzzleOutcome::Victory)
	{
		RecordCampaignWin();
	}
	return bAccepted;
}

void AGatherAndHuntPuzzleGameMode::RecordCampaignWin()
{
	if (CampaignNodeId.IsEmpty())
	{
		return;
	}
	FString Error;
	UGatherAndHuntCampaign* const Campaign = UGatherAndHuntCampaign::Load(this, Error);
	if (Campaign == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Campaign win for \"%s\" not recorded: %s"), *CampaignNodeId, *Error);
		return;
	}
	// A replayed level is already recorded and earns nothing new.
	if (Campaign->CompleteNode(CampaignNodeId))
	{
		CampaignRewardText = UGatherAndHuntCampaign::DescribeReward(Campaign->GetNodes()[Campaign->FindNodeIndex(CampaignNodeId)].Reward);
	}
	bAuthoringToolsAvailable = Campaign->IsNodeCompleted(0);
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
