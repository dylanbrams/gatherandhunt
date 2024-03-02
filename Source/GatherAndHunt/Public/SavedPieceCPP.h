// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PieceTypeEnum.h"
#include "Dom/JsonObject.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "JsonObjectConverter.h"
#include "SavedPieceCPP.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class GATHERANDHUNT_API USavedPieceCPP : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	PieceTypeEnum MyPieceTypeEnum;
    // Serialization functions can be added as needed

	UFUNCTION(BlueprintCallable)
	bool SerializeObjectToJsonString(USavedPieceCPP* Obj, FString& OutJsonString);

	virtual void Serialize(FArchive& Ar) override;


};
