// TurtleSim — pure-C++ simulation core. NO UE dependencies.
// This module wrapper exists so UBT can build the same source files that the
// CMake test build uses (Source/TurtleSim/CMakeLists.txt). The headers and
// implementations under Public/ and Private/ MUST NOT include UE types.

using UnrealBuildTool;
using System.IO;

public class TurtleSim : ModuleRules
{
	public TurtleSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		CppStandard = CppStandardVersion.Cpp20;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		// The simulation code itself stays UE-agnostic, but the module wrapper still
		// needs Core for the UE module implementation entry point.
		PublicDependencyModuleNames.Clear();
		PrivateDependencyModuleNames.Add("Core");
	}
}
