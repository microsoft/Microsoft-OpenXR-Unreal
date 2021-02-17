// Copyright (c) 2020 Microsoft Corporation.
// Licensed under the MIT License.

using System.IO;
using System.Linq;
using System.Runtime.Remoting.Messaging;
using UnrealBuildTool;
using Tools.DotNETCommon;
using System;
using System.Collections.Generic;

public class MicrosoftOpenXR : ModuleRules
{
	public MicrosoftOpenXR(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.HoloLens)
		{
			// these parameters mandatory for winrt support
			bEnableExceptions = true;
			bUseUnity = false;
			CppStandard = CppStandardVersion.Cpp17;
			PublicSystemLibraries.AddRange(new string[] { "shlwapi.lib", "runtimeobject.lib" });
		}

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = @"Private\OpenXRCommon.h";

		PrivateIncludePaths.AddRange(
			new string[] {
				// This private include path ensures our newer copy of the openxr headers take precedence over the engine's copy.
				"MicrosoftOpenXR/Private/External"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"OpenXRHMD",
				"MicrosoftOpenXRRuntimeSettings",
				"HeadMountedDisplay",
				"AugmentedReality",
				"OpenXRAR",
				"RHI",
				"RenderCore",
				"Projects",
				"NuGetModule"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"NuGetModule"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd"
				}
			);
		}

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"HeadMountedDisplay",
				"NuGetModule"
			}
			);

		PublicIncludePathModuleNames.AddRange(
			new string[]
			{
				"HeadMountedDisplay",
				"NuGetModule"
			}
			);
	}
}
