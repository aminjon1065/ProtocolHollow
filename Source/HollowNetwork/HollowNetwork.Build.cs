using UnrealBuildTool;

/** Layer 1: Steam sessions/lobbies/invites, voice, DLC entitlements, net diagnostics. Gameplay replication lives in its own domain modules. */
public class HollowNetwork : ModuleRules
{
	public HollowNetwork(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		// OnlineSubsystem / CoreOnline are public: session types appear in HollowSessionSubsystem.h.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"CoreOnline",
			"OnlineSubsystem",
			"HollowCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"OnlineSubsystemUtils"
		});

		// A Shipping build started outside the Steam client (the test builds of the Vertical Slice, AppID 480, ADR-0024) finds
		// its AppID in steam_appid.txt next to the executable: the launcher engine's Steam plugin is precompiled, so the
		// UE_PROJECT_STEAMSHIPPINGID define cannot reach it. A store release with its own AppID drops this file.
		if (Target.Configuration == UnrealTargetConfiguration.Shipping && Target.Type == TargetType.Game && Target.Platform == UnrealTargetPlatform.Win64)
		{
			RuntimeDependencies.Add("$(TargetOutputDir)/steam_appid.txt", "$(ProjectDir)/Build/Steam/steam_appid.txt", StagedFileType.NonUFS);
		}
	}
}
