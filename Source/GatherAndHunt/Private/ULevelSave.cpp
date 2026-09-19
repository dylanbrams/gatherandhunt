// Fill out your copyright notice in the Description page of Project Settings.


#include "ULevelSave.h"

namespace
{
	// A count or string length read from a legacy or malformed file can be
	// garbage; SetNum on it is fatal and a huge FString read logs a critical
	// error. Check against the bytes left (every element needs at least one).
	bool IsPlausibleCount(FArchive& Ar, const int64 Count)
	{
		return Count >= 0 && Count <= Ar.TotalSize() - Ar.Tell();
	}

	bool SafeReadString(FArchive& Ar, FString& Out)
	{
		const int64 Start = Ar.Tell();
		int32 SaveNum = 0;
		Ar << SaveNum;
		Ar.Seek(Start);
		// Negative lengths are UTF-16 (2 bytes per char).
		const int64 Bytes = SaveNum >= 0 ? static_cast<int64>(SaveNum) : -2 * static_cast<int64>(SaveNum);
		if (Ar.IsError() || !IsPlausibleCount(Ar, Bytes + static_cast<int64>(sizeof(int32))))
		{
			return false;
		}
		Ar << Out;
		return !Ar.IsError();
	}

	// Board files are raw tagged-property blobs with no version header, so older
	// ones must be recognised by shape. Returns false for the current layout.
	//  - No leading property-extension byte: saved before UE 5.2's tag extension.
	//  - Extension byte, but the first tag's size field follows the type name
	//    directly (non-zero): saved before 5.4's complete-type-name tags.
	bool DetectLegacyBoardFileVersion(const TArray<uint8>& Data, FPackageFileVersion& OutVersion)
	{
		auto ReadInt32 = [&Data](const int64 Offset, int32& Out)
		{
			if (Offset < 0 || Offset + 4 > Data.Num()) return false;
			FMemory::Memcpy(&Out, Data.GetData() + Offset, sizeof(int32));
			return true;
		};

		int32 FirstNameLength = 0;
		if (Data.Num() < 5 || (Data[0] != 0 && ReadInt32(0, FirstNameLength) && FirstNameLength > 0 && FirstNameLength < 256))
		{
			OutVersion = FPackageFileVersion(GPackageFileUEVersion.FileVersionUE4, EUnrealEngineObjectUE5Version::SCRIPT_SERIALIZATION_OFFSET);
			return Data.Num() >= 5;
		}

		// Skip the extension byte, the first tag's name, and its type name.
		int64 Offset = 1;
		for (int32 Skip = 0; Skip < 2; ++Skip)
		{
			int32 Length = 0;
			if (!ReadInt32(Offset, Length) || Length <= 0 || Length > 256) return false;
			Offset += 4 + Length;
		}
		int32 AfterTypeName = 0;
		if (ReadInt32(Offset, AfterTypeName) && AfterTypeName != 0)
		{
			OutVersion = FPackageFileVersion(GPackageFileUEVersion.FileVersionUE4, EUnrealEngineObjectUE5Version::PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION);
			return true;
		}
		return false;
	}

	bool SerializeScriptedOpponentTurns(FArchive& Ar, TArray<FScriptedOpponentTurnData>& ScriptedOpponentTurns)
	{
		int32 NumTurns = ScriptedOpponentTurns.Num();
		Ar << NumTurns;

		if (Ar.IsLoading())
		{
			if (!IsPlausibleCount(Ar, NumTurns))
			{
				return false;
			}
			ScriptedOpponentTurns.SetNum(NumTurns);
		}

		for (FScriptedOpponentTurnData& TurnData : ScriptedOpponentTurns)
		{
			Ar << TurnData.TurnNumber;

			int32 NumTurtlePlans = TurnData.TurtlePlans.Num();
			Ar << NumTurtlePlans;
			if (Ar.IsLoading())
			{
				if (!IsPlausibleCount(Ar, NumTurtlePlans))
				{
					return false;
				}
				TurnData.TurtlePlans.SetNum(NumTurtlePlans);
			}

			for (FScriptedPuzzleTurtlePlanData& TurtlePlanData : TurnData.TurtlePlans)
			{
				Ar << TurtlePlanData.TurtleId;

				int32 NumMoves = TurtlePlanData.Moves.Num();
				Ar << NumMoves;
				if (Ar.IsLoading())
				{
					if (!IsPlausibleCount(Ar, NumMoves))
					{
						return false;
					}
					TurtlePlanData.Moves.SetNum(NumMoves);
				}

				for (EGatherAndHuntPuzzleMoveKind& MoveKind : TurtlePlanData.Moves)
				{
					uint8 MoveAsByte = static_cast<uint8>(MoveKind);
					Ar << MoveAsByte;
					if (Ar.IsLoading())
					{
						MoveKind = static_cast<EGatherAndHuntPuzzleMoveKind>(MoveAsByte);
					}
				}
			}
		}
		return !Ar.IsError();
	}
}

int32 ULevelSave::GetNextAvailablePieceSaveID() const
{
	int32 MaxPieceSaveID = -1;
	for (const FPieceSaveData& Data : PieceSaveDataArray)
	{
		MaxPieceSaveID = FMath::Max(MaxPieceSaveID, Data.PieceSaveID);
	}

	return MaxPieceSaveID + 1;
}

bool ULevelSave::SerializeObjectToArchive(UObject* Object, const FString& Filename)
{
	if (!Object) return false;

	// Create a byte array to hold the serialized data
	TArray<uint8> ObjectData;
	FMemoryWriter MemoryWriter(ObjectData, true);

	// Use a proxy archive that allows us to serialize UObjects as if they were simple data
	FObjectAndNameAsStringProxyArchive Archive(MemoryWriter, false);
	if (Object)
	{
		Object->Serialize(Archive);
	}
	// Save the byte array to a file
	return FFileHelper::SaveArrayToFile(ObjectData, *Filename);
	
}

bool ULevelSave::LoadSerializedObjectFromFile(UObject* Object, const FString& Filename)
{
	if (!Object) return false;

	// Load the serialized data from the file into a byte array
	TArray<uint8> ObjectData;
	if (!FFileHelper::LoadFileToArray(ObjectData, *Filename)) return false;

	// Deserialize the object from the byte array
	FMemoryReader MemoryReader(ObjectData, true);
	FPackageFileVersion LegacyVersion;
	if (DetectLegacyBoardFileVersion(ObjectData, LegacyVersion))
	{
		MemoryReader.SetUEVer(LegacyVersion);
	}
	FObjectAndNameAsStringProxyArchive Archive(MemoryReader, true);

	ULevelSave* const LevelSave = Cast<ULevelSave>(Object);
	TGuardValue<bool> BoardFileGuard(LevelSave != nullptr ? LevelSave->bLoadingBoardFile : bLoadingBoardFile, true);
	Object->Serialize(Archive);
	return !Archive.IsError();
}


void ULevelSave::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Serialize a simple property

	// Explicitly serialize an array property
	if (Ar.IsSaving())
	{
		Ar << LevelName;
		Ar << LevelID;
		Ar << AvailableActionsID;
		Ar << GridSize;
		Ar << TargetObjectiveCount;
		Ar << TargetTurnCount;
		Ar << TargetScore;
		Ar << PuzzleAct;
		Ar << PuzzleOrder;
		Ar << IntroText;
		Ar << GridCellSize;
		SerializeScriptedOpponentTurns(Ar, ScriptedOpponentTurns);
		
		int32 ArraySize = PieceSaveDataArray.Num();
		Ar << ArraySize; // Save the size of the array
		for (FPieceSaveData& Data : PieceSaveDataArray)
		{
			Ar << Data.PieceSaveID;
			Ar << Data.PieceOwnerID;
			Ar << Data.Position;
			Ar << Data.Rotation;
			Ar << Data.Scale;
			Ar << Data.PieceType;
			Ar << Data.MovementType;
			Ar << Data.CanHaveQueue;
			Ar << Data.HazardStepDistance;
			Ar << Data.HazardPrimaryStepCount;
			Ar << Data.HazardSecondaryStepCount;
			Ar << Data.HazardPhaseOffset;
			// Serialize other properties as needed
		}
	}
	else if (Ar.IsLoading())
	{
		// Super::Serialize already restored every UPROPERTY from tagged data. The
		// manual trailer below repeats them; its layout changed several times in
		// the prototype, so only accept it if it parses cleanly to end of file.
		// Otherwise (legacy boards) keep the tagged values and skip the rest.
		// Other archives (e.g. DuplicateObject) always carry the current layout
		// and may hold more data after this object, so never skip ahead there.
		if (!LoadCurrentTrailer(Ar, bLoadingBoardFile) && bLoadingBoardFile)
		{
			UE_LOG(LogTemp, Log, TEXT("ULevelSave: '%s' is a legacy board file; using its tagged properties."), *LevelName);
			Ar.ClearError();
			Ar.Seek(Ar.TotalSize());
			// Legacy boards stored GridSize as the highest cell index; it is now in world units.
			GridSize *= GridCellSize;
		}
		SavedPieces.Empty();
	}
}

bool ULevelSave::LoadCurrentTrailer(FArchive& Ar, const bool bRequireEndOfArchive)
{
	FString InLevelName;
	int32 InLevelID = 0;
	int32 InAvailableActionsID = 0;
	FVector InGridSize = FVector::ZeroVector;
	int32 InTargetObjectiveCount = 0;
	int32 InTargetTurnCount = 0;
	int32 InTargetScore = 0;
	int32 InPuzzleAct = 0;
	int32 InPuzzleOrder = 0;
	FString InIntroText;
	float InGridCellSize = 0.0f;
	TArray<FScriptedOpponentTurnData> InScriptedOpponentTurns;

	if (!SafeReadString(Ar, InLevelName))
	{
		return false;
	}
	Ar << InLevelID;
	Ar << InAvailableActionsID;
	Ar << InGridSize;
	Ar << InTargetObjectiveCount;
	Ar << InTargetTurnCount;
	Ar << InTargetScore;
	Ar << InPuzzleAct;
	Ar << InPuzzleOrder;
	if (Ar.IsError() || !SafeReadString(Ar, InIntroText))
	{
		return false;
	}
	Ar << InGridCellSize;
	if (Ar.IsError() || !SerializeScriptedOpponentTurns(Ar, InScriptedOpponentTurns))
	{
		return false;
	}

	int32 ArraySize = 0;
	Ar << ArraySize;
	if (Ar.IsError() || !IsPlausibleCount(Ar, ArraySize))
	{
		return false;
	}
	TArray<FPieceSaveData> InPieces;
	InPieces.SetNum(ArraySize);
	for (FPieceSaveData& Data : InPieces)
	{
		Ar << Data.PieceSaveID;
		Ar << Data.PieceOwnerID;
		Ar << Data.Position;
		Ar << Data.Rotation;
		Ar << Data.Scale;
		Ar << Data.PieceType;
		Ar << Data.MovementType;
		Ar << Data.CanHaveQueue;
		Ar << Data.HazardStepDistance;
		Ar << Data.HazardPrimaryStepCount;
		Ar << Data.HazardSecondaryStepCount;
		Ar << Data.HazardPhaseOffset;
	}
	if (Ar.IsError() || (bRequireEndOfArchive && Ar.Tell() != Ar.TotalSize()))
	{
		return false;
	}

	LevelName = InLevelName;
	LevelID = InLevelID;
	AvailableActionsID = InAvailableActionsID;
	GridSize = InGridSize;
	TargetObjectiveCount = InTargetObjectiveCount;
	TargetTurnCount = InTargetTurnCount;
	TargetScore = InTargetScore;
	PuzzleAct = InPuzzleAct;
	PuzzleOrder = InPuzzleOrder;
	IntroText = InIntroText;
	GridCellSize = InGridCellSize;
	ScriptedOpponentTurns = MoveTemp(InScriptedOpponentTurns);
	PieceSaveDataArray = MoveTemp(InPieces);
	return true;
}
