// Copyright (c) Microsoft Corporation.

using UnrealBuildTool;
using System.Collections.Generic;

public class MsftOpenXRGameEditorTarget : TargetRules
{
	public MsftOpenXRGameEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.AddRange( new string[] { "MsftOpenXRGame" } );
	}
}
