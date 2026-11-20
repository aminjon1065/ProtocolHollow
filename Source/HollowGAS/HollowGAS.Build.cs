using UnrealBuildTool;

/** Layer 1: Ability System component, attribute sets, base abilities/effects/executions, shared character base. */
public class HollowGAS : ModuleRules
{
	public HollowGAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayTasks",
			"GameplayAbilities",
			"DeveloperSettings",
			"HollowCore"
		});
	}
}
