using UnrealBuildTool;

/** Layer 1: Ability System component, attribute sets, base abilities/effects/executions, shared character base, squad Resonance. */
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

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NetCore"   // push-model replication of the squad's Resonance
		});
	}
}
