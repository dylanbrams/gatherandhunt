#include "GatherAndHuntFrontEnd.h"

#include "GatherAndHuntLevelEditorLibrary.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "ULevelSave.h"

const TCHAR* const UGatherAndHuntFrontEnd::FrontEndMap = TEXT("/Engine/Maps/Entry");

FString UGatherAndHuntFrontEnd::GetBoardSetupsDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("BoardSetups"));
}

FString UGatherAndHuntFrontEnd::NormalizePuzzleFilename(const FString& RelativeFilename)
{
	// Spaces would break the ?Puzzle= travel option.
	FString File = RelativeFilename.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/")).Replace(TEXT(" "), TEXT("_"));
	while (File.StartsWith(TEXT("/")))
	{
		File.RightChopInline(1);
	}
	if (File.IsEmpty())
	{
		return File;
	}
	return File.EndsWith(TEXT(".dat")) ? File : File + TEXT(".dat");
}

bool UGatherAndHuntFrontEnd::PuzzleFileExists(const FString& RelativeFilename)
{
	const FString File = NormalizePuzzleFilename(RelativeFilename);
	return !File.IsEmpty() && IFileManager::Get().FileExists(*(GetBoardSetupsDir() / File));
}

TArray<FGatherAndHuntPuzzleListing> UGatherAndHuntFrontEnd::ListPuzzles(const bool bIncludeAutomation)
{
	const FString Dir = GetBoardSetupsDir();
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.dat"), true, false);

	TArray<FGatherAndHuntPuzzleListing> Listings;
	for (FString& File : Files)
	{
		FPaths::MakePathRelativeTo(File, *(Dir + TEXT("/")));
		if (!bIncludeAutomation && File.StartsWith(TEXT("Automation/")))
		{
			continue;
		}

		FGatherAndHuntPuzzleListing& Listing = Listings.AddDefaulted_GetRef();
		Listing.RelativeFilename = File;
		Listing.PuzzleName = FPaths::GetBaseFilename(File);
		if (const ULevelSave* const LevelSave = UGatherAndHuntLevelEditorLibrary::LoadLevelSaveFromProjectFile(GetTransientPackage(), File))
		{
			Listing.bLoaded = true;
			if (!LevelSave->LevelName.IsEmpty())
			{
				Listing.PuzzleName = LevelSave->LevelName;
			}
			Listing.Act = LevelSave->PuzzleAct;
			Listing.Order = LevelSave->PuzzleOrder;
			Listing.TargetTurnCount = LevelSave->TargetTurnCount;
			Listing.TargetScore = LevelSave->TargetScore;
			Listing.IntroText = LevelSave->IntroText;
		}
	}

	Listings.Sort([](const FGatherAndHuntPuzzleListing& A, const FGatherAndHuntPuzzleListing& B)
	{
		if (A.Act != B.Act) return A.Act < B.Act;
		if (A.Order != B.Order) return A.Order < B.Order;
		return A.RelativeFilename < B.RelativeFilename;
	});
	return Listings;
}

void UGatherAndHuntFrontEnd::OpenMainMenu(const UObject* WorldContextObject)
{
	UGameplayStatics::OpenLevel(WorldContextObject, FName(FrontEndMap), true,
		TEXT("game=/Script/GatherAndHunt.GatherAndHuntMenuGameMode"));
}

void UGatherAndHuntFrontEnd::OpenArtPreview(const UObject* WorldContextObject)
{
	UGameplayStatics::OpenLevel(WorldContextObject, FName(FrontEndMap), true,
		TEXT("game=/Script/GatherAndHunt.GatherAndHuntArtPreviewGameMode"));
}

void UGatherAndHuntFrontEnd::OpenCampaignMap(const UObject* WorldContextObject)
{
	UGameplayStatics::OpenLevel(WorldContextObject, FName(FrontEndMap), true,
		TEXT("game=/Script/GatherAndHunt.GatherAndHuntCampaignGameMode"));
}

void UGatherAndHuntFrontEnd::OpenCampaignPuzzle(const UObject* WorldContextObject, const FString& RelativeFilename, const FString& CampaignNodeId)
{
	UGameplayStatics::OpenLevel(WorldContextObject, FName(FrontEndMap), true, FString::Printf(
		TEXT("game=/Script/GatherAndHunt.GatherAndHuntPuzzleGameMode?Puzzle=%s?CampaignNode=%s"),
		*NormalizePuzzleFilename(RelativeFilename), *CampaignNodeId));
}

void UGatherAndHuntFrontEnd::OpenPuzzle(const UObject* WorldContextObject, const FString& RelativeFilename, const bool bEdit)
{
	FString Options = TEXT("game=/Script/GatherAndHunt.GatherAndHuntPuzzleGameMode");
	const FString File = NormalizePuzzleFilename(RelativeFilename);
	if (!File.IsEmpty())
	{
		Options += TEXT("?Puzzle=") + File;
	}
	if (bEdit)
	{
		Options += TEXT("?Edit");
	}
	UGameplayStatics::OpenLevel(WorldContextObject, FName(FrontEndMap), true, Options);
}
