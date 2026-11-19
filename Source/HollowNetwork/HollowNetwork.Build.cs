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
	}
}
