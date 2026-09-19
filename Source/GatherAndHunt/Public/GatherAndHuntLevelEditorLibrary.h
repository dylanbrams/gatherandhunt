#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PieceSaveData.h"
#include "ULevelSave.h"
#include "GatherAndHuntLevelEditorLibrary.generated.h"

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FLevelEditorValidationIssue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PieceSaveID = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsError = true;
};

UCLASS()
class GATHERANDHUNT_API UGatherAndHuntLevelEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|LevelEditor")
	static ULevelSave* CreateEmptyLevelSave(UObject* Outer, const FString& LevelName, int32 LevelID, FVector GridSize);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|LevelEditor")
	static bool UpsertPieceData(ULevelSave* LevelSave, FPieceSaveData PieceData);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|LevelEditor")
	static bool RemovePieceDataByID(ULevelSave* LevelSave, int32 PieceSaveID);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|LevelEditor")
	static void NormalizeLevelSave(ULevelSave* LevelSave);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|LevelEditor")
	static bool ValidateLevelSave(const ULevelSave* LevelSave, TArray<FLevelEditorValidationIssue>& OutIssues);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|LevelEditor")
	static bool SaveLevelSaveToProjectFile(ULevelSave* LevelSave, const FString& RelativeFilename);

	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|LevelEditor")
	static ULevelSave* LoadLevelSaveFromProjectFile(UObject* Outer, const FString& RelativeFilename);

	// Role of a turtle piece, from its owner and control flag.
	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|LevelEditor")
	static EGatherAndHuntTurtleRole GetTurtleRole(const FPieceSaveData& PieceData);

	// Sets PieceOwnerID and bScriptControlled to match Role.
	UFUNCTION(BlueprintCallable, Category = "GatherAndHunt|LevelEditor")
	static void SetTurtleRole(UPARAM(ref) FPieceSaveData& PieceData, EGatherAndHuntTurtleRole Role);

	UFUNCTION(BlueprintPure, Category = "GatherAndHunt|LevelEditor")
	static TArray<FVector> BuildHazardPreviewPath(const FPieceSaveData& PieceData, int32 SampleCount);
};
