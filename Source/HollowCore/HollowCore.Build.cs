using UnrealBuildTool;

/** Layer 0: logging, native GameplayTags, cross-module interfaces, asset manager, noise entry point, net/RNG helpers. */
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
			"DeveloperSettings"
		});
	}
}
