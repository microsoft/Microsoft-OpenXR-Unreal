// Copyright (c) Microsoft Corporation. All rights reserved.

using UnrealBuildTool;

public class MicrosoftOpenXRRuntimeSettings : ModuleRules
{
	public MicrosoftOpenXRRuntimeSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		if (Target.Type == TargetRules.TargetType.Editor || Target.Type == TargetRules.TargetType.Program)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"TargetPlatform"
				}
			);
		}
	}
}
