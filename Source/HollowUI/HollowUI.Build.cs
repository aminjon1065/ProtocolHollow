using UnrealBuildTool;

/** Layer 4: CommonUI foundation widgets, diegetic visor HUD, hub menus, view models. */
public class HollowUI : ModuleRules
{
	public HollowUI(ReadOnlyTargetRules Target) : base(Target)
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
			"UMG",
			"CommonUI",
			"CommonInput",
			"GameplayTags",
			"GameplayAbilities",
			"DeveloperSettings",
			"HollowCore",
			"HollowGAS",
			"HollowGame"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"HollowPhysics"   // automation tests drive the real door (the visor itself only knows Core interfaces)
		});
	}
}
