// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TeamProject_Fin : ModuleRules
{
	public TeamProject_Fin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"MotionWarping",
			"Niagara",
			"PhysicsCore",
			"UMG",
			"Slate",
			"SlateCore",
			"DeveloperSettings",
			"HTTP",
			"AudioModulation",
			"AIModule",
			"NavigationSystem",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"RenderCore",
			"RHI",
			"ChaosCaching",
			"CableComponent",
			"ProceduralMeshComponent",
			"LevelSequence",
			"MovieScene"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "FMODStudio", "AudioMixer" });

		PublicIncludePaths.AddRange(new string[] { ModuleDirectory });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
