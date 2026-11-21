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
#include "Sound/AudioSettings.h"
#include "Voice/HollowVoiceSettings.h"

namespace HollowBuildTests
{
	const TCHAR* const XcodeSection = TEXT("/Script/MacTargetPlatform.XcodeProjectSettings");
	const TCHAR* const ProjectSection = TEXT("/Script/EngineSettings.GeneralProjectSettings");
	const TCHAR* const SteamSection = TEXT("OnlineSubsystemSteam");
	const TCHAR* const EngineDefaultPrefix = TEXT("com.YourCompany");
	const TCHAR* const SandboxKey = TEXT("com.apple.security.app-sandbox");
	const TCHAR* const SteamAppIdFile = TEXT("Build/Steam/steam_appid.txt");
	const TCHAR* const InfoPlist = TEXT("Build/Mac/Resources/Info.Template.plist");
	const TCHAR* const MicrophoneUsageKey = TEXT("<key>NSMicrophoneUsageDescription</key>");
	const TCHAR* const AudioInputKey = TEXT("com.apple.security.device.audio-input");
	const TCHAR* const CookSection = TEXT("/Script/UnrealEd.ProjectPackagingSettings");

	/** How often Needle occurs in Text. */
	int32 Occurrences(const FString& Text, const TCHAR* Needle)
	{
		int32 Count = 0;
		for (int32 At = Text.Find(Needle); At != INDEX_NONE; At = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
		{
			++Count;
		}
		return Count;
	}
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

	// Proximity voice (S4-07, ADR-0025): macOS asks for the microphone with the game's reason; the entitlement the
	// hardened runtime will need is there; the engine's VOIP is on and waits for the player's push-to-talk.
	FString Plist;
	TestTrue(TEXT("Info.Template.plist explains the microphone, once"), FFileHelper::LoadFileToString(Plist, *FPaths::Combine(FPaths::ProjectDir(), InfoPlist))
		&& Occurrences(Plist, MicrophoneUsageKey) == 1);
	TestEqual(TEXT("...and has no key twice"), Occurrences(Plist, TEXT("<key>NSHumanReadableCopyright</key>")) + Occurrences(Plist, TEXT("<key>LSApplicationCategoryType</key>")), 2);
	TestTrue(TEXT("Entitlements: audio input"), Text.Contains(AudioInputKey));
	bool bVoice = false;
	bool bVoiceModule = false;
	bool bPushToTalk = false;
	TestTrue(TEXT("The online subsystem carries voice"), GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bHasVoiceEnabled"), bVoice, GEngineIni) && bVoice);
	TestTrue(TEXT("The voice module is on"), GConfig->GetBool(TEXT("Voice"), TEXT("bEnabled"), bVoiceModule, GEngineIni) && bVoiceModule);
	TestTrue(TEXT("Nobody transmits on joining (push-to-talk, the player's mode)"),
		GConfig->GetBool(TEXT("/Script/Engine.GameSession"), TEXT("bRequiresPushToTalk"), bPushToTalk, GGameIni) && bPushToTalk);
	TArray<FString> AlwaysCook;
	GConfig->GetArray(CookSection, TEXT("+DirectoriesToAlwaysCook"), AlwaysCook, GGameIni);
	if (AlwaysCook.IsEmpty())
	{
		GConfig->GetArray(CookSection, TEXT("DirectoriesToAlwaysCook"), AlwaysCook, GGameIni);
	}
	const FString VoiceConfig = GetDefault<UHollowVoiceSettings>()->VoiceConfig.ToSoftObjectPath().GetLongPackageName();
	const FString SoundClass = GetDefault<UAudioSettings>()->VoiPSoundClass.GetLongPackageName();
	auto Cooked = [&AlwaysCook](const FString& Package)
	{
		return !Package.IsEmpty() && AlwaysCook.ContainsByPredicate([&Package](const FString& Entry)
		{
			FString Directory;
			return FParse::Value(*Entry, TEXT("Path="), Directory) && Package.StartsWith(Directory.TrimQuotes() + TEXT("/"));
		});
	};
	TestTrue(FString::Printf(TEXT("DA_VoiceConfig is cooked (%s)"), *VoiceConfig), Cooked(VoiceConfig));
	TestTrue(FString::Printf(TEXT("...and the VOIP sound class (%s)"), *SoundClass), Cooked(SoundClass));

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
