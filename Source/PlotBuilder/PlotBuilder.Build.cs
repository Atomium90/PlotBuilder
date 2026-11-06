// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PlotBuilder : ModuleRules
{
	public PlotBuilder(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate"
		});

		// ProceduralMeshComponent: the mission ground is a mesh built from a height function at the start of the level.
		PrivateDependencyModuleNames.AddRange(new string[] { "GeometryCollectionEngine", "ProceduralMeshComponent" });

		PublicIncludePaths.AddRange(new string[] {
			"PlotBuilder"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
