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
			"UMG",
			"CommonUI",
			"CommonInput",
			"HollowCore",
			"HollowGame"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
