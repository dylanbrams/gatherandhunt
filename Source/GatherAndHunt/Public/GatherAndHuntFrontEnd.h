#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GatherAndHuntFrontEnd.generated.h"

// A board file under Content/BoardSetups and the metadata the menu shows.
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FGatherAndHuntPuzzleListing
{
	GENERATED_BODY()

	// Relative to Content/BoardSetups, with the .dat extension.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|FrontEnd")
	FString RelativeFilename;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|FrontEnd")
	FString PuzzleName;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|FrontEnd")
	int32 Act = 1;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|FrontEnd")
	int32 Order = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|FrontEnd")
	int32 TargetTurnCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|FrontEnd")
	int32 TargetScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|FrontEnd")
	FString IntroText;

	// False if the file didn't load; the name then is the filename.
	UPROPERTY(BlueprintReadOnly, Category = "GatherAndHunt|FrontEnd")
	bool bLoaded = false;
};

// Menu and travel helpers. Every screen runs on the engine's empty Entry map
// with a C++ GameMode picked on the URL, so no map or Blueprint assets are
// involved.
UCLASS()
class GATHERANDHUNT_API UGatherAndHuntFrontEnd : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Board files, sorted by act, order, then filename. Automation fixtures
	// (Automation/) are left out unless asked for.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|FrontEnd")
	static TArray<FGatherAndHuntPuzzleListing> ListPuzzles(bool bIncludeAutomation = false);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|FrontEnd", meta = (WorldContext = "WorldContextObject"))
	static void OpenMainMenu(const UObject* WorldContextObject);

	// The ship on a turntable, for looking at art outside a puzzle.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|FrontEnd", meta = (WorldContext = "WorldContextObject"))
	static void OpenArtPreview(const UObject* WorldContextObject);

	// bEdit starts in the board editor; a file that doesn't exist yet then
	// becomes a new, empty puzzle saved under that name.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|FrontEnd", meta = (WorldContext = "WorldContextObject"))
	static void OpenPuzzle(const UObject* WorldContextObject, const FString& RelativeFilename, bool bEdit);

	// The level select map (see UGatherAndHuntCampaign).
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|FrontEnd", meta = (WorldContext = "WorldContextObject"))
	static void OpenCampaignMap(const UObject* WorldContextObject);

	// A level entered from the map: winning it finishes CampaignNodeId, and
	// leaving it goes back to the map.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|FrontEnd", meta = (WorldContext = "WorldContextObject"))
	static void OpenCampaignPuzzle(const UObject* WorldContextObject, const FString& RelativeFilename, const FString& CampaignNodeId);

	// "My board" -> "My_board.dat": adds the extension, trims leading slashes
	// and turns spaces into underscores.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|FrontEnd")
	static FString NormalizePuzzleFilename(const FString& RelativeFilename);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|FrontEnd")
	static bool PuzzleFileExists(const FString& RelativeFilename);

	static FString GetBoardSetupsDir();

	// The map every front-end screen loads.
	static const TCHAR* const FrontEndMap;
};
