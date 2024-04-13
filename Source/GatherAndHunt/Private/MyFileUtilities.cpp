#include "MyFileUtilities.h"



TArray<FString> UMyFileUtilities::GetFilesInDirectory(const FString& DirectoryPath, const FString& ExtensionFilter)
{
	TArray<FString> FileNames;
	IFileManager& FileManager = IFileManager::Get();

	// Generate the search path, filtering by extension if provided
	const FString FinalPath = DirectoryPath + "/*." + ExtensionFilter;
	FileManager.FindFiles(FileNames, *FinalPath, true, false);

	return FileNames;
}
