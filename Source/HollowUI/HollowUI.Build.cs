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
			"HollowEconomy",
			"HollowNetwork",
			"HollowGame"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"HollowPhysics",   // the objective follows the Aether cores (UX-01); tests drive the real door
			"HollowAI",        // NAR-01a: the radio's helmet sensor knows the Echo-Stalker
			"HollowPCG"        // NAR-01a: the radio reads the raid's sector from its manifest
		});
	}
}
