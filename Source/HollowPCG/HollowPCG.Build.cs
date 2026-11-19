using UnrealBuildTool;

/** Layer 1: deterministic sector generator (pure C++), room/biome/contract data, sector manifest, runtime assembly, in-room dressing. */
public class HollowPCG : ModuleRules
{
	public HollowPCG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"GameplayTags",
			"ModularGameplay",
			"HollowCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"PCG",
			"NavigationSystem",
			"NetCore"
		});
	}
}
