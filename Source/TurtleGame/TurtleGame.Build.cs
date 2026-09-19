// TurtleGame — UE module that wraps TurtleSim with replication, GameMode/State,
// and player controllers. The simulation core (TurtleSim) is consumed via plain
// C++ headers; this module owns the UE↔sim adapter layer.

using UnrealBuildTool;
using System.IO;

public class TurtleGame : ModuleRules
{
	public TurtleGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
			"TurtleSim",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
		});
	}
}
