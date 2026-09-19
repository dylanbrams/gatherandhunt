#include "GatherAndHuntLevelEditorLibrary.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
	void AddIssue(
		TArray<FLevelEditorValidationIssue>& OutIssues,
		const FString& Message,
		const int32 PieceSaveID,
		const bool bIsError)
	{
		FLevelEditorValidationIssue Issue;
		Issue.Message = Message;
		Issue.PieceSaveID = PieceSaveID;
		Issue.bIsError = bIsError;
		OutIssues.Add(Issue);
	}

	bool IsHazardPieceType(const PieceTypeEnum PieceType)
	{
		return PieceType == PieceTypeEnum::BP_Hazard
			|| PieceType == PieceTypeEnum::BP_CircularHazard
			|| PieceType == PieceTypeEnum::BP_LinearPatrolHazard;
	}

	void NormalizeHazardAuthoring(FPieceSaveData& PieceData)
	{
		PieceData.HazardStepDistance = FMath::Max(1.0f, PieceData.HazardStepDistance);
		PieceData.HazardPrimaryStepCount = FMath::Max(1, PieceData.HazardPrimaryStepCount);
		PieceData.HazardSecondaryStepCount = FMath::Max(1, PieceData.HazardSecondaryStepCount);
	}

	FString ResolveBoardSetupPath(const FString& RelativeFilename)
	{
		const FString Filename = RelativeFilename.EndsWith(TEXT(".dat"))
			? RelativeFilename
			: RelativeFilename + TEXT(".dat");
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("BoardSetups") / Filename);
	}
}

ULevelSave* UGatherAndHuntLevelEditorLibrary::CreateEmptyLevelSave(
	UObject* Outer,
	const FString& LevelName,
	const int32 LevelID,
	const FVector GridSize)
{
	UObject* const SafeOuter = Outer != nullptr ? Outer : GetTransientPackage();

	ULevelSave* const LevelSave = NewObject<ULevelSave>(SafeOuter);
	LevelSave->LevelName = LevelName;
	LevelSave->LevelID = LevelID;
	LevelSave->GridSize = FVector(
		FMath::Max(1.0f, GridSize.X),
		FMath::Max(1.0f, GridSize.Y),
		FMath::Max(1.0f, GridSize.Z));
	return LevelSave;
}

bool UGatherAndHuntLevelEditorLibrary::UpsertPieceData(ULevelSave* LevelSave, FPieceSaveData PieceData)
{
	if (LevelSave == nullptr)
	{
		return false;
	}

	if (PieceData.PieceSaveID < 0)
	{
		PieceData.PieceSaveID = LevelSave->GetNextAvailablePieceSaveID();
	}

	NormalizeHazardAuthoring(PieceData);

	for (FPieceSaveData& ExistingPieceData : LevelSave->PieceSaveDataArray)
	{
		if (ExistingPieceData.PieceSaveID == PieceData.PieceSaveID)
		{
			ExistingPieceData = PieceData;
			return true;
		}
	}

	LevelSave->PieceSaveDataArray.Add(PieceData);
	return true;
}

bool UGatherAndHuntLevelEditorLibrary::RemovePieceDataByID(ULevelSave* LevelSave, const int32 PieceSaveID)
{
	if (LevelSave == nullptr)
	{
		return false;
	}

	const int32 RemovedCount = LevelSave->PieceSaveDataArray.RemoveAll(
		[PieceSaveID](const FPieceSaveData& PieceData)
		{
			return PieceData.PieceSaveID == PieceSaveID;
		});
	return RemovedCount > 0;
}

void UGatherAndHuntLevelEditorLibrary::NormalizeLevelSave(ULevelSave* LevelSave)
{
	if (LevelSave == nullptr)
	{
		return;
	}

	LevelSave->GridSize = FVector(
		FMath::Max(1.0f, LevelSave->GridSize.X),
		FMath::Max(1.0f, LevelSave->GridSize.Y),
		FMath::Max(1.0f, LevelSave->GridSize.Z));

	LevelSave->TargetObjectiveCount = FMath::Max(0, LevelSave->TargetObjectiveCount);
	LevelSave->TargetTurnCount = FMath::Max(0, LevelSave->TargetTurnCount);

	LevelSave->PieceSaveDataArray.Sort(
		[](const FPieceSaveData& Left, const FPieceSaveData& Right)
		{
			return Left.PieceSaveID < Right.PieceSaveID;
		});

	TSet<int32> SeenPieceIDs;
	for (FPieceSaveData& PieceData : LevelSave->PieceSaveDataArray)
	{
		if (PieceData.PieceSaveID < 0 || SeenPieceIDs.Contains(PieceData.PieceSaveID))
		{
			PieceData.PieceSaveID = LevelSave->GetNextAvailablePieceSaveID();
		}
		SeenPieceIDs.Add(PieceData.PieceSaveID);
		NormalizeHazardAuthoring(PieceData);
	}
}

bool UGatherAndHuntLevelEditorLibrary::ValidateLevelSave(
	const ULevelSave* LevelSave,
	TArray<FLevelEditorValidationIssue>& OutIssues)
{
	OutIssues.Reset();

	if (LevelSave == nullptr)
	{
		AddIssue(OutIssues, TEXT("LevelSave is null."), INDEX_NONE, true);
		return false;
	}

	if (LevelSave->LevelName.TrimStartAndEnd().IsEmpty())
	{
		AddIssue(OutIssues, TEXT("LevelName is empty."), INDEX_NONE, true);
	}

	if (LevelSave->GridSize.X < 1.0f || LevelSave->GridSize.Y < 1.0f || LevelSave->GridSize.Z < 1.0f)
	{
		AddIssue(OutIssues, TEXT("GridSize must be positive on all axes."), INDEX_NONE, true);
	}

	if (LevelSave->TargetObjectiveCount < 0)
	{
		AddIssue(OutIssues, TEXT("TargetObjectiveCount cannot be negative."), INDEX_NONE, true);
	}

	if (LevelSave->TargetTurnCount < 0)
	{
		AddIssue(OutIssues, TEXT("TargetTurnCount cannot be negative."), INDEX_NONE, true);
	}

	TSet<int32> SeenPieceIDs;
	int32 ObjectiveCount = 0;

	for (const FPieceSaveData& PieceData : LevelSave->PieceSaveDataArray)
	{
		if (PieceData.PieceSaveID < 0)
		{
			AddIssue(OutIssues, TEXT("PieceSaveID must be non-negative."), PieceData.PieceSaveID, true);
		}
		else if (SeenPieceIDs.Contains(PieceData.PieceSaveID))
		{
			AddIssue(OutIssues, TEXT("Duplicate PieceSaveID detected."), PieceData.PieceSaveID, true);
		}
		else
		{
			SeenPieceIDs.Add(PieceData.PieceSaveID);
		}

		if (PieceData.Position.X < 0.0f || PieceData.Position.Y < 0.0f || PieceData.Position.Z < 0.0f
			|| PieceData.Position.X > LevelSave->GridSize.X
			|| PieceData.Position.Y > LevelSave->GridSize.Y
			|| PieceData.Position.Z > LevelSave->GridSize.Z)
		{
			AddIssue(OutIssues, TEXT("Piece position is outside GridSize bounds."), PieceData.PieceSaveID, true);
		}

		if (PieceData.PieceType == PieceTypeEnum::BP_Objective
			|| PieceData.PieceType == PieceTypeEnum::BP_GoldObjective)
		{
			++ObjectiveCount;
		}

		if (PieceData.IsDeterministicHazard() || IsHazardPieceType(PieceData.PieceType))
		{
			if (PieceData.HazardStepDistance <= 0.0f)
			{
				AddIssue(OutIssues, TEXT("HazardStepDistance must be positive."), PieceData.PieceSaveID, true);
			}
			if (PieceData.HazardPrimaryStepCount <= 0)
			{
				AddIssue(OutIssues, TEXT("HazardPrimaryStepCount must be at least 1."), PieceData.PieceSaveID, true);
			}
			if (PieceData.IsCircularHazard() && PieceData.HazardSecondaryStepCount <= 0)
			{
				AddIssue(OutIssues, TEXT("Circular hazards require HazardSecondaryStepCount >= 1."), PieceData.PieceSaveID, true);
			}
		}
	}

	if (LevelSave->TargetObjectiveCount > ObjectiveCount)
	{
		AddIssue(OutIssues, TEXT("TargetObjectiveCount exceeds authored objective count."), INDEX_NONE, true);
	}

	for (const FLevelEditorValidationIssue& Issue : OutIssues)
	{
		if (Issue.bIsError)
		{
			return false;
		}
	}

	return true;
}

bool UGatherAndHuntLevelEditorLibrary::SaveLevelSaveToProjectFile(ULevelSave* LevelSave, const FString& RelativeFilename)
{
	if (LevelSave == nullptr)
	{
		return false;
	}

	NormalizeLevelSave(LevelSave);

	const FString AbsoluteFilename = ResolveBoardSetupPath(RelativeFilename);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilename), true);
	return LevelSave->SerializeObjectToArchive(LevelSave, AbsoluteFilename);
}

ULevelSave* UGatherAndHuntLevelEditorLibrary::LoadLevelSaveFromProjectFile(UObject* Outer, const FString& RelativeFilename)
{
	UObject* const SafeOuter = Outer != nullptr ? Outer : GetTransientPackage();
	ULevelSave* const LevelSave = NewObject<ULevelSave>(SafeOuter);
	const FString AbsoluteFilename = ResolveBoardSetupPath(RelativeFilename);

	if (!LevelSave->LoadSerializedObjectFromFile(LevelSave, AbsoluteFilename))
	{
		return nullptr;
	}

	NormalizeLevelSave(LevelSave);
	return LevelSave;
}

TArray<FVector> UGatherAndHuntLevelEditorLibrary::BuildHazardPreviewPath(
	const FPieceSaveData& PieceData,
	const int32 SampleCount)
{
	TArray<FVector> Path;
	if (!PieceData.IsDeterministicHazard())
	{
		Path.Add(PieceData.Position);
		return Path;
	}

	const int32 PathLength = FMath::Max(1, PieceData.GetDeterministicHazardPathLength());
	const int32 TurnsToSample = SampleCount > 0 ? SampleCount : PathLength;
	Path.Reserve(TurnsToSample);

	for (int32 TurnIndex = 0; TurnIndex < TurnsToSample; ++TurnIndex)
	{
		Path.Add(PieceData.GetDeterministicHazardPositionAtTurn(TurnIndex));
	}

	return Path;
}
