// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CityGIS : ModuleRules
{
	public CityGIS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "WebBrowser", "WebBrowserWidget", "UMG", "Json", "JsonUtilities"  });
	}
}
