using UnrealBuildTool;

/** Layer 2: enemies, AI controllers, StateTree tasks, EQS, perception, Threat Director, activity heatmap, vent network. */
public class HollowAI : ModuleRules
{
	public HollowAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",
			"NavigationSystem",
			"GameplayTags",
			"GameplayAbilities",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"HollowCore",
			"HollowGAS"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayTasks",
			"MotionWarping"
		});
	}
}
