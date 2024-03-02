// Fill out your copyright notice in the Description page of Project Settings.


#include "SavedPieceCPP.h"
#include "Serialization/Archive.h"


bool IsTransformDefault(const FTransform& Transform)
{
	return Transform.Equals(FTransform::Identity);
}

bool USavedPieceCPP::SerializeObjectToJsonString(USavedPieceCPP* Obj, FString& OutJsonString)
{
	if (Obj == nullptr)
	{
		return false;
	}

	FJsonObjectConverter::UStructToJsonObjectString(Obj->GetClass(), Obj, OutJsonString, 0, 0);
	return true;
}

void USavedPieceCPP::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	uint8 EnumAsByte = static_cast<uint8>(MyPieceTypeEnum);
	Ar << EnumAsByte;
	MyPieceTypeEnum = static_cast<PieceTypeEnum>(EnumAsByte);

	if (Ar.IsSaving())
	{
		// Serialize each member variable
		if (!IsTransformDefault(Transform))
			Transform.Serialize(Ar);

		// Serializing the enum
	
	}

	// Deserializing the enum
	if (Ar.IsLoading())
	{
		Transform.Serialize(Ar);
	}
	// For more complex or custom types (like other UObjects), you might need custom serialization logic
}
