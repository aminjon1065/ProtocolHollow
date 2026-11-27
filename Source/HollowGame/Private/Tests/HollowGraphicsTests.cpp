#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SpotLightComponent.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Scalability.h"
#include "Sector/HollowSectorSettings.h"
#include "Settings/HollowGameUserSettings.h"
#include "Settings/HollowGraphicsSettings.h"

namespace HollowGraphicsTests
{
	const TCHAR* const PackagingSection = TEXT("/Script/UnrealEd.ProjectPackagingSettings");
	const TCHAR* const MapsSection = TEXT("/Script/EngineSettings.GameMapsSettings");
	const TCHAR* const AbilitySection = TEXT("/Script/GameplayAbilities.AbilitySystemGlobals");
	/** Lumen's final gather methods (r.Lumen.FinalGatherMethod): 0 Irradiance Field (Lumen Lite), 1 Screen Probe Gather. */
	constexpr int32 IrradianceField = 0;
	constexpr int32 ScreenProbes = 1;
	constexpr float FullResolution = 100.f;

	int32 CVarInt(const TCHAR* Name)
	{
		const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
		return Variable ? Variable->GetInt() : INDEX_NONE;
	}

	/** Whether MapsToCook (DefaultGame.ini) holds the package of an object path. */
	bool IsCooked(const TArray<FString>& Maps, const FString& ObjectPath)
	{
		const FString Package = FPackageName::ObjectPathToPackageName(ObjectPath);
		return !Package.IsEmpty() && Maps.ContainsByPredicate([&Package](const FString& Entry) { return Entry.Contains(FString::Printf(TEXT("\"%s\""), *Package)); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGraphicsPresetsTest, "Hollow.Game.Graphics.Presets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGraphicsPresetsTest::RunTest(const FString& Parameters)
{
	// The two presets (ADR-0023): High is the E9 target at full resolution, Low runs Lumen Lite (Irradiance Field gather,
	// GI still on) upscaled with dynamic resolution; a weak benchmark score or a Steam Deck picks Low. Applying a preset
	// sets its scalability level and the project's cvars for it (DefaultScalability.ini); the editor's own are restored.
	using namespace HollowGraphicsTests;
	const UHollowGraphicsSettings* Graphics = GetDefault<UHollowGraphicsSettings>();
	const FHollowGraphicsPresetSpec* Low = Graphics->FindPreset(EHollowGraphicsPreset::Low);
	const FHollowGraphicsPresetSpec* High = Graphics->FindPreset(EHollowGraphicsPreset::High);
	if (!TestNotNull(TEXT("Low preset"), Low) || !TestNotNull(TEXT("High preset"), High))
	{
		return false;
	}
	TestTrue(TEXT("High: a higher scalability level than Low"), High->ScalabilityLevel > Low->ScalabilityLevel);
	TestTrue(TEXT("High: full resolution, no dynamic resolution (E9 as measured)"), High->ScreenPercentage == FullResolution && !High->bDynamicResolution);
	TestTrue(TEXT("Low: upscaled with dynamic resolution"), Low->ScreenPercentage < FullResolution && Low->bDynamicResolution
		&& Low->MinScreenPercentage <= Low->ScreenPercentage && Low->DynamicResolutionBudgetMs > 0.f);
	TestTrue(TEXT("A weak GPU gets Low, a strong one High"), Graphics->PresetForBenchmark(Graphics->LowGpuBenchmarkScore * 0.5f) == EHollowGraphicsPreset::Low
		&& Graphics->PresetForBenchmark(Graphics->LowGpuBenchmarkScore) == EHollowGraphicsPreset::High);
	TestTrue(TEXT("Steam Deck: its variable, resolution and frame limit are data"), !Graphics->DeckEnvironmentVariable.IsEmpty()
		&& Graphics->DeckResolution.GetMin() > 0 && Graphics->DeckFrameRateLimit > 0.f);

	const FString DeckBefore = FPlatformMisc::GetEnvironmentVariable(*Graphics->DeckEnvironmentVariable);
	FPlatformMisc::SetEnvironmentVar(*Graphics->DeckEnvironmentVariable, TEXT("1"));
	TestTrue(TEXT("SteamDeck=1: a Steam Deck"), UHollowGameUserSettings::IsSteamDeck());
	FPlatformMisc::SetEnvironmentVar(*Graphics->DeckEnvironmentVariable, *DeckBefore);
	TestEqual(TEXT("...not otherwise"), UHollowGameUserSettings::IsSteamDeck(), DeckBefore == TEXT("1"));

	const Scalability::FQualityLevels Before = Scalability::GetQualityLevels();
	TArray<TPair<FString, FString>> SavedVariables;
	for (const TCHAR* Name : {TEXT("r.DynamicRes.MinScreenPercentage"), TEXT("r.DynamicRes.MaxScreenPercentage"), TEXT("r.DynamicRes.FrameTimeBudget")})
	{
		SavedVariables.Emplace(Name, IConsoleManager::Get().FindConsoleVariable(Name)->GetString());
	}
	TestTrue(TEXT("Low applied"), UHollowGameUserSettings::ApplyScalability(*Low));
	TestEqual(TEXT("Low: Lumen Lite (Irradiance Field gather)"), CVarInt(TEXT("r.Lumen.FinalGatherMethod")), IrradianceField);
	TestEqual(TEXT("...with Lumen GI still on"), CVarInt(TEXT("r.Lumen.DiffuseIndirect.Allow")), 1);
	TestEqual(TEXT("...volumetric fog kept (the project's override of level 1)"), CVarInt(TEXT("r.VolumetricFog")), 1);
	const int32 LowFogCell = CVarInt(TEXT("r.VolumetricFog.GridPixelSize"));
	TestEqual(TEXT("...characters one LOD coarser (CHAR-01: the project's override of level 1)"), CVarInt(TEXT("r.SkeletalMeshLODBias")), 2);
	TestTrue(TEXT("High applied"), UHollowGameUserSettings::ApplyScalability(*High));
	TestEqual(TEXT("High: characters at their full LOD"), CVarInt(TEXT("r.SkeletalMeshLODBias")), 0);
	TestEqual(TEXT("High: Screen Probe Gather"), CVarInt(TEXT("r.Lumen.FinalGatherMethod")), ScreenProbes);
	TestTrue(TEXT("Low's fog grid is coarser than High's"), LowFogCell > CVarInt(TEXT("r.VolumetricFog.GridPixelSize")));
	Scalability::SetQualityLevels(Before, /*bForce=*/true);
	for (const TPair<FString, FString>& Saved : SavedVariables)
	{
		IConsoleManager::Get().FindConsoleVariable(*Saved.Key)->Set(*Saved.Value, ECVF_SetByGameSetting);
	}
	IConsoleManager::Get().CallAllConsoleVariableSinks();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGraphicsCookMapsTest, "Hollow.Game.Graphics.CookMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGraphicsCookMapsTest::RunTest(const FString& Parameters)
{
	// A packaged build (the E9 run is one) must cook what is reached by path only: the raid map, the transition map and
	// the GameplayCue notifies (found by GameplayCueNotifyPaths; uncooked, a packaged game shows no tool beam).
	using namespace HollowGraphicsTests;
	TArray<FString> Maps;
	GConfig->GetArray(PackagingSection, TEXT("MapsToCook"), Maps, GGameIni);
	FString TransitionMap;
	GConfig->GetString(MapsSection, TEXT("TransitionMap"), TransitionMap, GEngineIni);
	const FString RaidMap = GetDefault<UHollowSectorSettings>()->RaidMap.ToString();
	TestTrue(FString::Printf(TEXT("The raid map is cooked (%s)"), *RaidMap), IsCooked(Maps, RaidMap));
	TestTrue(FString::Printf(TEXT("The transition map is cooked (%s)"), *TransitionMap), IsCooked(Maps, TransitionMap));
	TArray<FString> CuePaths;
	TArray<FString> AlwaysCooked;
	GConfig->GetArray(AbilitySection, TEXT("GameplayCueNotifyPaths"), CuePaths, GGameIni);
	GConfig->GetArray(PackagingSection, TEXT("DirectoriesToAlwaysCook"), AlwaysCooked, GGameIni);
	TestFalse(TEXT("The project has GameplayCue paths"), CuePaths.IsEmpty());
	for (const FString& Path : CuePaths)
	{
		TestTrue(FString::Printf(TEXT("GameplayCue path %s is always cooked"), *Path),
			AlwaysCooked.ContainsByPredicate([&Path](const FString& Entry) { return Entry.Contains(FString::Printf(TEXT("\"%s\""), *Path)); }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGraphicsHelmetShadowsTest, "Hollow.Game.Graphics.HelmetShadows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGraphicsHelmetShadowsTest::RunTest(const FString& Parameters)
{
	// Moving shadowed lights cost the most (Block 3 sec. 3.9): only this machine's own helmet lamp casts shadows.
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Own = HollowGameTests::SpawnExoFrame(*this, World);
	AHollowExoFrameCharacter* Other = HollowGameTests::SpawnExoFrame(*this, World, FTransform(FVector(0.0, 300.0, 0.0)));
	if (!Own || !Other)
	{
		return false;
	}
	TestFalse(TEXT("A copy no one on this machine controls: no shadows"), Own->GetHelmetLight()->CastShadows != 0);
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	HollowGameTests::PossessAsPlayer(World, Own, /*bLocalPlayer=*/true);
	HollowGameTests::PossessAsPlayer(World, Other, /*bLocalPlayer=*/false);
	TestTrue(TEXT("This machine's player: its lamp casts shadows"), Own->GetHelmetLight()->CastShadows != 0);
	TestFalse(TEXT("Another player's lamp: no shadows"), Other->GetHelmetLight()->CastShadows != 0);
	Own->GetController()->UnPossess();
	TestFalse(TEXT("No longer this machine's: no shadows"), Own->GetHelmetLight()->CastShadows != 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGraphicsDisplayTest, "Hollow.Game.Graphics.Display",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHollowGraphicsDisplayTest::RunTest(const FString& Parameters)
{
	// S4-06 bug bash: a fullscreen window needs a known monitor (otherwise the Mac window is placed below the screen and
	// takes no clicks). The settings remember the primary one, never override a choice, and do nothing without monitors.
	auto MakeMonitor = [](const TCHAR* Id, const bool bPrimary)
	{
		FMonitorInfo Monitor;
		Monitor.ID = Id;
		Monitor.Name = Id;
		Monitor.bIsPrimary = bPrimary;
		return Monitor;
	};
	FDisplayMetrics Metrics;
	Metrics.MonitorInfo.Add(MakeMonitor(TEXT("Side"), false));
	Metrics.MonitorInfo.Add(MakeMonitor(TEXT("Main"), true));
	TestEqual(TEXT("None chosen: the primary monitor"), UHollowGameUserSettings::DisplayToChoose(Metrics, FString(), INDEX_NONE), 1);
	TestEqual(TEXT("A chosen index stays"), UHollowGameUserSettings::DisplayToChoose(Metrics, FString(), 0), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("A chosen ID stays"), UHollowGameUserSettings::DisplayToChoose(Metrics, TEXT("Side"), INDEX_NONE), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("No monitors: nothing to choose"), UHollowGameUserSettings::DisplayToChoose(FDisplayMetrics(), FString(), INDEX_NONE), static_cast<int32>(INDEX_NONE));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
