#pragma once
#include "Runtime/Core/Public/HAL/FileManagerGeneric.h"
#include "MyFileUtilities.generated.h"

UCLASS()
class GATHERANDHUNT_API UMyFileUtilities : public UBlueprintFunctionLibrary {
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FileUtilities")
	static TArray<FString> GetFilesInDirectory(const FString& DirectoryPath, const FString& ExtensionFilter);

};
