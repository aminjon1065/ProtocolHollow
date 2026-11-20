#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/HollowFrontEndGameMode.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Raid/HollowRaidLauncher.h"
#include "Session/HollowNetworkSettings.h"

namespace HollowBuildTests
{
	const TCHAR* const XcodeSection = TEXT("/Script/MacTargetPlatform.XcodeProjectSettings");
	const TCHAR* const ProjectSection = TEXT("/Script/EngineSettings.GeneralProjectSettings");
	const TCHAR* const SteamSection = TEXT("OnlineSubsystemSteam");
	const TCHAR* const EngineDefaultPrefix = TEXT("com.YourCompany");
	const TCHAR* const SandboxKey = TEXT("com.apple.security.app-sandbox");
	const TCHAR* const SteamAppIdFile = TEXT("Build/Steam/steam_appid.txt");
	/** "/Game/..." in a FilePath of the Xcode settings is the project folder. */
	const TCHAR* const GamePrefix = TEXT("/Game/");

	/** The file a FilePath setting such as (FilePath="/Game/Build/Mac/Resources/X.entitlements") names, on disk. */
	FString FileOf(const FString& Setting)
	{
		FString Path;
		if (!FParse::Value(*Setting, TEXT("FilePath="), Path) || !Path.StartsWith(GamePrefix))
		{
			return FString();
		}
		return FPaths::Combine(FPaths::ProjectDir(), Path.RightChop(FCString::Strlen(GamePrefix)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBuildSettingsTest, "Hollow.Game.Build.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBuildSettingsTest::RunTest(const FString& Parameters)
{
	// The packaged Steam build (S4-06, ADR-0024): the project's own bundle prefix and identity; Mac signed with one
	// entitlements file without App Sandbox in both configurations; the Steam AppID of the config and of steam_appid.txt
	// agree; the front-end map runs the front-end game mode (the main menu, no body).
	using namespace HollowBuildTests;
	FString Prefix;
	GConfig->GetString(XcodeSection, TEXT("CodeSigningPrefix"), Prefix, GEngineIni);
	TestTrue(FString::Printf(TEXT("Bundle prefix is the project's (%s)"), *Prefix), !Prefix.IsEmpty() && Prefix != EngineDefaultPrefix);
	FString Company;
	FString Copyright;
	GConfig->GetString(ProjectSection, TEXT("CompanyName"), Company, GGameIni);
	GConfig->GetString(ProjectSection, TEXT("CopyrightNotice"), Copyright, GGameIni);
	TestTrue(TEXT("Company and copyright set"), !Company.IsEmpty() && !Copyright.IsEmpty());

	FString Development;
	FString Shipping;
	GConfig->GetString(XcodeSection, TEXT("PremadeMacEntitlements"), Development, GEngineIni);
	GConfig->GetString(XcodeSection, TEXT("ShippingSpecificMacEntitlements"), Shipping, GEngineIni);
	const FString Entitlements = FileOf(Development);
	FString Text;
	TestTrue(FString::Printf(TEXT("Development and Shipping sign with the same project file (%s)"), *Entitlements),
		!Entitlements.IsEmpty() && FileOf(Shipping) == Entitlements && FFileHelper::LoadFileToString(Text, *Entitlements));
	TestFalse(TEXT("...without App Sandbox (Steam, network, Saved)"), Text.Contains(SandboxKey));

	int32 DevAppId = 0;
	bool bRelaunch = true;
	FString FileAppId;
	TestTrue(TEXT("Steam: a development AppID"), GConfig->GetInt(SteamSection, TEXT("SteamDevAppId"), DevAppId, GEngineIni) && DevAppId > 0);
	TestTrue(TEXT("...no relaunch through Steam in development"), GConfig->GetBool(SteamSection, TEXT("bRelaunchInSteam"), bRelaunch, GEngineIni) && !bRelaunch);
	TestTrue(TEXT("...steam_appid.txt (Shipping test builds) holds the same AppID"),
		FFileHelper::LoadFileToString(FileAppId, *FPaths::Combine(FPaths::ProjectDir(), SteamAppIdFile)) && FCString::Atoi(*FileAppId.TrimStartAndEnd()) == DevAppId);

	const UWorld* FrontEnd = GetDefault<UHollowNetworkSettings>()->FrontEndMap.LoadSynchronous();
	const AWorldSettings* WorldSettings = FrontEnd && FrontEnd->PersistentLevel ? FrontEnd->PersistentLevel->GetWorldSettings() : nullptr;
	TestTrue(TEXT("The front-end map runs the front-end game mode"), WorldSettings && WorldSettings->DefaultGameMode
		&& WorldSettings->DefaultGameMode->IsChildOf(AHollowFrontEndGameMode::StaticClass()));
	TestTrue(TEXT("The hub is where a raid starts"), HollowRaidLauncher::IsHubMap(GetDefault<UHollowNetworkSettings>()->HubMap.GetLongPackageName()));
	TestFalse(TEXT("...not the front end"), HollowRaidLauncher::IsHubMap(GetDefault<UHollowNetworkSettings>()->FrontEndMap.GetLongPackageName()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
