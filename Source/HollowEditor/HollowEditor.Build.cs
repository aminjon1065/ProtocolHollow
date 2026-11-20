using UnrealBuildTool;

/** Editor-only: data validation, content authoring helpers for the Python scripts (widgets, geometry collections, StateTrees, EQS). */
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
			"AssetTools",
			"DataValidation",
			"Slate",
			"SlateCore",
			"UMG",
			"UMGEditor",
			"AssetRegistry",
			"Chaos",
			"GeometryCollectionEngine",
			"DataflowCore",
			"PlanarCut",
			"FractureEngine",
			"HollowCore",
			"HollowGAS",
			"HollowPCG",
			"HollowPhysics",
			"HollowAI",
			"HollowGame",
			"GameplayTags",
			"Json",
			"StateTreeModule",
			"StateTreeEditorModule",
			"PropertyBindingUtils",
			"GameplayStateTreeModule",
			"AIModule"
		});
	}
}
