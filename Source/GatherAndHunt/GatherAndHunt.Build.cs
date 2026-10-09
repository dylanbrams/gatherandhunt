// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class GatherAndHunt : ModuleRules
{
	public GatherAndHunt(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The widget .cpp files share helper names in their anonymous namespaces
		// (RoleLabel, the panel colours), which collide when merged into one unity file.
		bUseUnity = false;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "DeveloperSettings", "Engine", "EnhancedInput", "InputCore", "JsonUtilities", "Json", "TurtleSim", "UMG" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
