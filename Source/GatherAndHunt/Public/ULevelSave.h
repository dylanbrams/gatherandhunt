// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SavedPieceCPP.h"
#include "PieceSaveData.h"
#include "UObject/Object.h"
#include "MISC/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "ULevelSave.generated.h"

/**
 * 
 */
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
	int32 AvailableActionsID;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector GridSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<USavedPieceCPP*> SavedPieces;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FPieceSaveData> PieceSaveDataArray;
	
	UFUNCTION(BlueprintCallable)
	bool SerializeObjectToArchive(UObject* Object, const FString& Filename);

	UFUNCTION(BlueprintCallable)
	bool LoadSerializedObjectFromFile(UObject* Object, const FString& Filename);

	UPROPERTY()
	TArray<TSoftObjectPtr<USavedPieceCPP>> SoftSavedPieces;
	
	virtual void Serialize(FArchive& Ar) override;
	

	
};
