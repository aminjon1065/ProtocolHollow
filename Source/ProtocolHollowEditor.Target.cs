using UnrealBuildTool;

public class ProtocolHollowEditorTarget : TargetRules
{
	public ProtocolHollowEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		ProtocolHollowTarget.ApplySharedSettings(this);
		ExtraModuleNames.AddRange(ProtocolHollowTarget.RuntimeModules);
		ExtraModuleNames.Add("HollowEditor");
	}
}
