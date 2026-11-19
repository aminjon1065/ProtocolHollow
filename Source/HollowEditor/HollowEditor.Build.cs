using UnrealBuildTool;

/** Editor-only: data validation, component visualizers, sector preview. */
public class HollowEditor : ModuleRules
{
	public HollowEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"DataValidation",
			"HollowCore",
			"HollowGAS",
			"HollowPCG",
			"HollowPhysics",
			"HollowGame"
		});
	}
}
