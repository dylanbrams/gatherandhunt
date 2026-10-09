#include "GatherAndHuntArtSettings.h"

#include "GatherAndHuntPuzzlePieceActor.h"
#include "Materials/MaterialInterface.h"

UMaterialInterface* UGatherAndHuntArtSettings::LoadSkinForRole(const EGatherAndHuntTurtleRole TurtleRole) const
{
	const FName SkinName = TurtleRole == EGatherAndHuntTurtleRole::Ally ? AllySkin
		: TurtleRole == EGatherAndHuntTurtleRole::Opponent ? OpponentSkin
		: PlayerSkin;
	for (const FGatherAndHuntShipSkin& Skin : ShipSkins)
	{
		if (Skin.Name == SkinName)
		{
			return Skin.Material.LoadSynchronous();
		}
	}
	return nullptr;
}

void UGatherAndHuntArtSettings::AddShipGlows(AGatherAndHuntPuzzlePieceActor* Piece, const float FitSize) const
{
	for (const FGatherAndHuntShipGlow& Glow : ShipGlows)
	{
		if (UMaterialInterface* const Material = Glow.Material.LoadSynchronous())
		{
			Piece->AddGlow(Material, Glow.MeshLocation, Glow.Facing, Glow.Size * FitSize, Glow.MinScreenSize, Glow.bShowCloseUp);
		}
	}
}
