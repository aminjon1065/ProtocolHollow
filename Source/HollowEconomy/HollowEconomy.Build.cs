using UnrealBuildTool;

/** Layer 2: item/recipe definitions, FastArray inventory, crafting rules, insurance, extraction resolver, local profile. */
public class HollowEconomy : ModuleRules
{
	public HollowEconomy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"GameplayAbilities",   // the raid inventory grants its items' abilities (ADR-0030)
			"GameplayTags",
			"NetCore",
			"HollowCore",
			"HollowGAS"
		});
	}
}
