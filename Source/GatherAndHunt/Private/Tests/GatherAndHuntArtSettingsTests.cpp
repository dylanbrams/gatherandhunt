#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/StaticMesh.h"
#include "GatherAndHuntArtSettings.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGatherAndHuntArtSettingsResolveTest,
	"GatherAndHunt.GatherAndHunt.Art.SettingsResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Catches art that was moved or renamed without updating DefaultGame.ini; the
// board would otherwise fall back to placeholder shapes without a word.
bool FGatherAndHuntArtSettingsResolveTest::RunTest(const FString&)
{
	const UGatherAndHuntArtSettings* const Art = GetDefault<UGatherAndHuntArtSettings>();
	if (Art->ShipMesh.IsNull())
	{
		return true; // No ship art configured: placeholders are intended.
	}

	TestNotNull(TEXT("Ship mesh loads"), Art->ShipMesh.LoadSynchronous());
	for (const FGatherAndHuntShipSkin& Skin : Art->ShipSkins)
	{
		TestNotNull(*FString::Printf(TEXT("Skin %s loads"), *Skin.Name.ToString()), Skin.Material.LoadSynchronous());
	}
	for (const FGatherAndHuntShipGlow& Glow : Art->ShipGlows)
	{
		TestNotNull(*FString::Printf(TEXT("Glow %s loads"), *Glow.Name.ToString()), Glow.Material.LoadSynchronous());
	}
	TestNotNull(TEXT("Player skin resolves"), Art->LoadSkinForRole(EGatherAndHuntTurtleRole::Player));
	TestNotNull(TEXT("Ally skin resolves"), Art->LoadSkinForRole(EGatherAndHuntTurtleRole::Ally));
	TestNotNull(TEXT("Opponent skin resolves"), Art->LoadSkinForRole(EGatherAndHuntTurtleRole::Opponent));
	return true;
}

#endif
