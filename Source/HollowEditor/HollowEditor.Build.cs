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
			"Engine",
			"DeveloperSettings"
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
			"Localization",   // LOC-01: the tests read the Game target's manifest and archives (FLocTextHelper)
			"StateTreeModule",
			"StateTreeEditorModule",
			"PropertyBindingUtils",
			"GameplayStateTreeModule",
			"AIModule",
			"Niagara",
			"NiagaraCore",
			"NiagaraEditor",   // Art-01: the beam's sparks are built from the content scripts (UHollowNiagaraAuthoring)
			// UI-02 (UI2-5): the tests render every MetaSound offline (no audio device), as the mixer would pull it.
			"MetasoundEngine",
			"MetasoundFrontend",
			"MetasoundGenerator",
			"MetasoundGraphCore"
		});
	}
}
