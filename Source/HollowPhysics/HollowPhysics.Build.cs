using UnrealBuildTool;

/** Layer 2: door/weld/cut state machines, Chaos breach, heavy-core carrying, gravity zones, barricades, destruction budget, nav links, CMC. */
public class HollowPhysics : ModuleRules
{
	public HollowPhysics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",
			"Chaos",
			"GeometryCollectionEngine",
			"NavigationSystem",
			"GameplayTags",
			"GameplayAbilities",
			"HollowCore",
			"HollowGAS"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ChaosSolverEngine",
			"FieldSystemEngine",
			"ChaosCaching",
			"NetCore"
		});
	}
}
