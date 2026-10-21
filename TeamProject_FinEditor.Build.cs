// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TeamProject_FinEditor : ModuleRules
{
	public TeamProject_FinEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AnimationModifiers",
			"AnimationBlueprintLibrary",
			"TeamProject_Fin",
		});

		// 선택 액터 등록 창과 에디터 Tools 메뉴에만 사용한다.
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "InputCore", "ToolMenus" });
	}
}
