// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SavedPieceCPP.h"
#include "PieceSaveData.h"
#include "GatherAndHuntPuzzleRuntimeTypes.h"
#include "UObject/Object.h"
#include "MISC/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "ULevelSave.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FScriptedPuzzleTurtlePlanData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TurtleId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EGatherAndHuntPuzzleMoveKind> Moves;
};

USTRUCT(BlueprintType)
struct GATHERANDHUNT_API FScriptedOpponentTurnData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TurnNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FScriptedPuzzleTurtlePlanData> TurtlePlans;
};

UCLASS(BlueprintType)
class GATHERANDHUNT_API ULevelSave : public UObject
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LevelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 LevelID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TargetObjectiveCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TargetTurnCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TargetScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PuzzleAct = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PuzzleOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString IntroText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FScriptedOpponentTurnData> ScriptedOpponentTurns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AvailableActionsID;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector GridSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GridCellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<USavedPieceCPP*> SavedPieces;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FPieceSaveData> PieceSaveDataArray;

	UFUNCTION(BlueprintPure)
	int32 GetNextAvailablePieceSaveID() const;
	
	UFUNCTION(BlueprintCallable)
	bool SerializeObjectToArchive(UObject* Object, const FString& Filename);

	UFUNCTION(BlueprintCallable)
	bool LoadSerializedObjectFromFile(UObject* Object, const FString& Filename);

	UPROPERTY()
	TArray<TSoftObjectPtr<USavedPieceCPP>> SoftSavedPieces;
	
	virtual void Serialize(FArchive& Ar) override;

private:
	// Reads the current-format manual trailer; false (and nothing changed) if the
	// bytes don't match it, e.g. boards saved by the original prototype. With
	// bRequireEndOfArchive the trailer must also end exactly at end of file.
	// bOutVersioned reports whether the trailer carried a version header.
	bool LoadCurrentTrailer(FArchive& Ar, bool bRequireEndOfArchive, bool& bOutVersioned);

	// True only while LoadSerializedObjectFromFile deserializes a board file.
	bool bLoadingBoardFile = false;
	

	
};
