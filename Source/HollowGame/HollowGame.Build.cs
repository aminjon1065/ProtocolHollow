using UnrealBuildTool;

/** Layer 3 (primary game module): game framework, ExoFrame character, tool abilities, interaction, raid flow, extraction, hub. */
public class HollowGame : ModuleRules
{
	public HollowGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"GameplayAbilities",
			"ModularGameplay",
			"HollowCore",
			"HollowGAS",
			"HollowEconomy",
			"HollowNetwork",
			"HollowPhysics",
			"HollowPCG",
			"HollowAI"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AIModule",
			"ApplicationCore",   // FDisplayMetrics: the game window's monitor (S4-06)
			"DeveloperSettings",
			"GameFeatures",
			"NavigationSystem",
			"NetCore",
			"AudioModulation",
			"AudioGameplay",
			"AudioGameplayVolume",
			"Json",
			"Niagara",   // the tool beam's sparks (Art-01)
			"RHI",
			"StateTreeModule",
			"GameplayStateTreeModule"
		});
	}
}
