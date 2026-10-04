// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class TndaFighterEditorTarget : TargetRules
{
	public TndaFighterEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("TndaFighter");
	}
}
