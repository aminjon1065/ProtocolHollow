using UnrealBuildTool;

public class ProtocolHollowTarget : TargetRules
{
	/** Runtime modules of the project, bottom-up (Docs/Architecture/01_Module_and_Plugin_Map.md). Shared with the editor target. */
	public static readonly string[] RuntimeModules =
	{
		"HollowCore",
		"HollowGAS",
		"HollowEconomy",
		"HollowNetwork",
		"HollowPhysics",
		"HollowPCG",
		"HollowAI",
		"HollowGame",
		"HollowUI"
	};

	public ProtocolHollowTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		ApplySharedSettings(this);
		ExtraModuleNames.AddRange(RuntimeModules);
	}

	/** Engine is locked to UE 5.8.x (ADR-0008): pin build settings so an engine hotfix never silently changes them. */
	public static void ApplySharedSettings(TargetRules Rules)
	{
		Rules.DefaultBuildSettings = BuildSettingsVersion.V7;
		Rules.IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
	}
}
