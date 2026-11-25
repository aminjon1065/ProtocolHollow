using UnrealBuildTool;

/** Layer 0: logging, native GameplayTags, cross-module interfaces, asset manager, noise entry point, net/RNG helpers, data FX. */
public class HollowCore : ModuleRules
{
	public HollowCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AIModule",
			"DeveloperSettings",
			"Niagara"   // FAB-01: HollowFx plays the data's Niagara effects (ADR-0051)
		});
	}
}
