// Copyright (c) 2020 Microsoft Corporation.
// Licensed under the MIT License.

using UnrealBuildTool;
using System.Collections.Generic;

public class MsftOpenXRGameTarget : TargetRules
{
	public MsftOpenXRGameTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.AddRange( new string[] { "MsftOpenXRGame" } );
	}
}
