// Fill out your copyright notice in the Description page of Project Settings.


#include "ULevelSave.h"




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
	FObjectAndNameAsStringProxyArchive Archive(MemoryReader, true);
	Object->Serialize(Archive);
	return true;
}


void ULevelSave::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Serialize a simple property

	// Explicitly serialize an array property
	if (Ar.IsSaving())
	{
		Ar << GridSize;
		int32 ArraySize = SavedPieces.Num();
		Ar << ArraySize; // Save the size of the array
		for (FPieceSaveData& Data : PieceSaveDataArray)
		{
			Ar << Data.PieceSaveID;
			Ar << Data.PieceOwnerID;
			Ar << Data.Position;
			Ar << Data.Rotation;
			Ar << Data.Scale;
			Ar << Data.PieceType;
			// Serialize other properties as needed
		}
	}
	else if (Ar.IsLoading())
	{
	
		Ar << GridSize;
		int32 ArraySize;
		Ar << ArraySize; // Load the size of the array
		SavedPieces.Empty(); // Ensure the array is empty before loading
		SavedPieces.Reserve(ArraySize);
		for (FPieceSaveData& Data : PieceSaveDataArray)
		{
			Ar << Data.PieceSaveID;
			Ar << Data.PieceOwnerID;
			Ar << Data.Position;
			Ar << Data.Rotation;
			Ar << Data.Scale;
			Ar << Data.PieceType;
			// Serialize other properties as needed
		}
	}
}

