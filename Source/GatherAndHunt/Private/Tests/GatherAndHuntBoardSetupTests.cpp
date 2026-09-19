#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GatherAndHuntLevelEditorLibrary.h"
#include "GatherAndHuntPuzzleSession.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntBoardSetupsLoadTest,
	"GatherAndHunt.GatherAndHunt.BoardSetups.AllFilesLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGatherAndHuntBoardSetupsLoadTest::RunTest(const FString&)
{
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("BoardSetups"));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.dat"), true, false);
	TestTrue(TEXT("Board setups exist"), Files.Num() > 0);

	for (FString File : Files)
	{
		FPaths::MakePathRelativeTo(File, *(Dir + TEXT("/")));
		ULevelSave* const LevelSave = UGatherAndHuntLevelEditorLibrary::LoadLevelSaveFromProjectFile(GetTransientPackage(), File);
		if (!TestNotNull(*FString::Printf(TEXT("%s deserializes"), *File), LevelSave))
		{
			continue;
		}

		UGatherAndHuntPuzzleSession* const Session = NewObject<UGatherAndHuntPuzzleSession>();
		TestTrue(*FString::Printf(TEXT("%s converts to a board"), *File), Session->LoadPuzzleFromLevelSave(LevelSave));
		FBox Extents(ForceInit);
		for (const FPieceSaveData& Piece : LevelSave->PieceSaveDataArray)
		{
			Extents += Piece.Position;
		}
		AddInfo(FString::Printf(TEXT("%s: '%s', %d pieces, GridSize %s, cell %.0f, piece extents %s, grid %s"),
			*File, *LevelSave->LevelName, LevelSave->PieceSaveDataArray.Num(),
			*LevelSave->GridSize.ToString(), LevelSave->GridCellSize, *Extents.ToString(),
			*Session->GetCurrentBoardSnapshot().GridSize.ToString()));
	}
	return true;
}

#endif
