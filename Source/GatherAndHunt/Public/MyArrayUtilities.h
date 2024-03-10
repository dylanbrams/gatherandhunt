// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyArrayUtilities.generated.h"

/**
 * 
 */
UCLASS()
class GATHERANDHUNT_API UMyArrayUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ArrayUtilities")
	static void SortIntArray(UPARAM(ref) TArray<int32>& ArrayToSort, bool bAscending = true);
	
	UFUNCTION(BlueprintCallable, Category = "ArrayUtilities")
	static TArray<int32> SplitAndRearrangeArray(TArray<int32> InputArray, int32 SplitValue);	UMyArrayUtilities();
	~UMyArrayUtilities();
};

