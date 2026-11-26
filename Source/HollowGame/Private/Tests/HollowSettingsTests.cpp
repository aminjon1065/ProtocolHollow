#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "EnhancedActionKeyMapping.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Input/HollowLookSettingsModifier.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Scalability.h"
#include "Settings/HollowAudioVolumeSubsystem.h"
#include "Settings/HollowGameUserSettings.h"
#include "Settings/HollowGraphicsSettings.h"
#include "Settings/HollowPlayerSettingsConfig.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "SoundControlBus.h"
#include "SoundModulationParameter.h"
#include "Tests/HollowGameTestUtils.h"

namespace HollowSettingsTests
{
	const TCHAR* const RaidInput = TEXT("/Game/Hollow/Core/Input/IMC_Raid.IMC_Raid");
	const TCHAR* const WorldClass = TEXT("/Game/Hollow/Audio/SC_Hollow_World.SC_Hollow_World");
	const TCHAR* const UIClass = TEXT("/Game/Hollow/Audio/SC_Hollow_UI.SC_Hollow_UI");
	const TCHAR* const VoiceClass = TEXT("/Game/Hollow/Audio/Voice/SC_Hollow_Voice.SC_Hollow_Voice");
	const TCHAR* const AudioRoot = TEXT("/Game/Hollow/Audio");
	const TCHAR* const UISoundPrefix = TEXT("MS_UI_");
	const TCHAR* const MetaSoundSourceClass = TEXT("MetaSoundSource");
	constexpr float Tolerance = 1e-3f;

	/**
	 * A settings object of the test's own that saves into a file of Saved/Automation (the player's GameUserSettings.ini
	 * stays untouched) and is read back by a second object, as the next launch would.
	 */
	struct FTestSettings
	{
		explicit FTestSettings(const TCHAR* Name)
			: File(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / FString::Printf(TEXT("%s.ini"), Name)))
		{
			IFileManager::Get().Delete(*File);
			Settings.Reset(NewObject<UHollowGameUserSettings>(GetTransientPackage()));
			Settings->SetConfigFileForTests(File);
		}

		~FTestSettings()
		{
			IFileManager::Get().Delete(*File);
		}

		FTestSettings(const FTestSettings&) = delete;
		FTestSettings& operator=(const FTestSettings&) = delete;

		/** What the next launch reads. */
		UHollowGameUserSettings* Reload() const
		{
			Settings->SaveSettings();
			UHollowGameUserSettings* Next = NewObject<UHollowGameUserSettings>(GetTransientPackage());
			Next->LoadConfig(nullptr, *File);
			return Next;
		}

		const FString File;
		TStrongObjectPtr<UHollowGameUserSettings> Settings;
	};

	bool IsVolumeBus(const USoundControlBus* Bus)
	{
		return Bus && Bus->Parameter && Bus->Parameter->IsA<USoundModulationParameterVolume>();
	}

	TSet<FName> VolumeBuses(const USoundClass* SoundClass)
	{
		TSet<FName> Names;
		if (SoundClass)   // (bEnableModulation is the editor's: the game reads the modulators)
		{
			for (const TObjectPtr<USoundModulatorBase>& Modulator : SoundClass->Properties.ModulationSettings.VolumeModulationDestination.Modulators)
			{
				if (Modulator)
				{
					Names.Add(Modulator->GetFName());
				}
			}
		}
		return Names;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSettingsControlsTest, "Hollow.Game.Settings.Controls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSettingsControlsTest::RunTest(const FString& Parameters)
{
	// UI-02 (ADR-0052): the mouse sensitivity (0.2-3.0), the inverted look and the field of view (75-105) stay in their
	// ranges, are kept for the next launch and apply: the look settings modifier on both look mappings of IMC_Raid (the
	// mouse takes the sensitivity, both the inversion), the field of view on the camera of this machine's own ExoFrame.
	using namespace HollowSettingsTests;
	const UHollowPlayerSettingsConfig* Config = GetDefault<UHollowPlayerSettingsConfig>();
	const FHollowSettingRange& Sensitivity = Config->MouseSensitivity;
	const FHollowSettingRange& Fov = Config->FieldOfView;
	TestTrue(TEXT("Sensitivity 0.2..3.0 by 0.1, 1 by default"), FMath::IsNearlyEqual(Sensitivity.Min, 0.2f) && FMath::IsNearlyEqual(Sensitivity.Max, 3.f)
		&& FMath::IsNearlyEqual(Sensitivity.Step, 0.1f) && FMath::IsNearlyEqual(Sensitivity.Default, 1.f));
	TestTrue(TEXT("Field of view 75..105 degrees, 90 by default"), FMath::IsNearlyEqual(Fov.Min, 75.f) && FMath::IsNearlyEqual(Fov.Max, 105.f)
		&& FMath::IsNearlyEqual(Fov.Default, 90.f));
	TestEqual(TEXT("Below the range: its floor"), Sensitivity.Snap(0.05f), 0.2f, Tolerance);
	TestEqual(TEXT("Above it: its ceiling"), Sensitivity.Snap(9.f), 3.f, Tolerance);
	TestEqual(TEXT("Between steps: the nearest"), Sensitivity.Snap(1.74f), 1.7f, Tolerance);
	TestEqual(TEXT("A step up"), Sensitivity.Stepped(1.f, 1), 1.1f, Tolerance);
	TestEqual(TEXT("No step past the top"), Sensitivity.Stepped(3.f, 1), 3.f, Tolerance);

	{
		FTestSettings Test(TEXT("HollowSettingsControls"));
		int32 Changes = 0;
		Test.Settings->OnControlsChanged.AddLambda([&Changes]() { ++Changes; });
		Test.Settings->SetMouseSensitivity(1.74f);
		Test.Settings->SetFieldOfView(140.f);
		Test.Settings->SetLookInverted(true);
		TestEqual(TEXT("Sensitivity snapped"), Test.Settings->GetMouseSensitivity(), 1.7f, Tolerance);
		TestEqual(TEXT("Field of view at most 105"), Test.Settings->GetFieldOfView(), 105.f, Tolerance);
		TestEqual(TEXT("Every change told"), Changes, 3);
		const UHollowGameUserSettings* Next = Test.Reload();
		TestTrue(TEXT("The next launch has them"), FMath::IsNearlyEqual(Next->GetMouseSensitivity(), 1.7f, Tolerance)
			&& FMath::IsNearlyEqual(Next->GetFieldOfView(), 105.f, Tolerance) && Next->IsLookInverted());
	}

	// The modifier's arithmetic, then the input data it sits in.
	const FVector2D Look = UHollowLookSettingsModifier::Apply(FVector2D(2.0, 3.0), 1.5f, /*bInverted=*/true);
	TestTrue(TEXT("Sensitivity scales both axes, the inversion flips the vertical"), Look.Equals(FVector2D(3.0, -4.5), Tolerance));
	TestTrue(TEXT("Not inverted: the vertical as it came"), UHollowLookSettingsModifier::Apply(FVector2D(1.0, 1.0), 1.f, false).Equals(FVector2D(1.0, 1.0)));
	const UInputMappingContext* Input = LoadObject<UInputMappingContext>(nullptr, RaidInput);
	if (!TestNotNull(TEXT("IMC_Raid"), Input))
	{
		return false;
	}
	int32 LookMappings = 0;
	for (const FEnhancedActionKeyMapping& Mapping : Input->GetMappings())
	{
		if (!Mapping.Action || Mapping.Action->GetFName() != TEXT("IA_Look"))
		{
			continue;
		}
		const TObjectPtr<UInputModifier>* Found = Mapping.Modifiers.FindByPredicate([](const UInputModifier* Modifier) { return Modifier && Modifier->IsA<UHollowLookSettingsModifier>(); });
		const UHollowLookSettingsModifier* Settings = Found ? Cast<UHollowLookSettingsModifier>(Found->Get()) : nullptr;
		const bool bMouse = Mapping.Key == EKeys::Mouse2D;
		TestTrue(FString::Printf(TEXT("IA_Look on %s takes the look settings (%s)"), *Mapping.Key.ToString(), bMouse ? TEXT("with the sensitivity") : TEXT("the inversion only")),
			Settings && Settings->bApplySensitivity == bMouse);
		++LookMappings;
	}
	TestEqual(TEXT("IMC_Raid: the mouse's and the right stick's look"), LookMappings, 2);

	// The field of view on this machine's own ExoFrame, now and on a change.
	UHollowGameUserSettings* Engine = UHollowGameUserSettings::Get();
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(*this, World);
	if (!TestNotNull(TEXT("The engine's settings"), Engine) || !TestNotNull(TEXT("ExoFrame"), Character))
	{
		return false;
	}
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);   // the bare C++ ExoFrame
	const float Kept = Engine->GetFieldOfView();
	Engine->SetFieldOfView(Fov.Max, /*bSave=*/false);
	HollowGameTests::PossessAsPlayer(World, Character, /*bLocalPlayer=*/true);
	const UCameraComponent* Camera = Character->GetFirstPersonCamera();
	TestEqual(TEXT("The local camera takes the settings' field of view"), Camera->FieldOfView, Fov.Max, Tolerance);
	Engine->SetFieldOfView(Fov.Min, /*bSave=*/false);
	TestEqual(TEXT("...and follows a change"), Camera->FieldOfView, Fov.Min, Tolerance);
	Engine->SetFieldOfView(Kept, /*bSave=*/false);
	Character->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSettingsAudioTest, "Hollow.Game.Settings.Audio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSettingsAudioTest::RunTest(const FString& Parameters)
{
	// UI-02 (ADR-0052): three volumes (master, effects, voice) in 5 % steps, kept for the next launch, each an Audio
	// Modulation bus the sound classes listen to: the world and the interface the master and effects buses, the voices
	// and the radio the master and voice buses. Every MetaSound of the game plays through one of those classes, the UI's
	// through SC_Hollow_UI. On this machine the volume subsystem sets the buses to the settings, at once and on a change.
	using namespace HollowSettingsTests;
	const UHollowPlayerSettingsConfig* Config = GetDefault<UHollowPlayerSettingsConfig>();
	const FHollowSettingRange& Range = Config->Volume;
	TestTrue(TEXT("Volumes 0..1 by 0.05, full by default"), FMath::IsNearlyEqual(Range.Min, 0.f) && FMath::IsNearlyEqual(Range.Max, 1.f)
		&& FMath::IsNearlyEqual(Range.Step, 0.05f) && FMath::IsNearlyEqual(Range.Default, 1.f));
	{
		FTestSettings Test(TEXT("HollowSettingsAudio"));
		int32 Changes = 0;
		Test.Settings->OnAudioChanged.AddLambda([&Changes]() { ++Changes; });
		Test.Settings->SetVolume(EHollowVolume::Master, 1.3f);
		Test.Settings->SetVolume(EHollowVolume::Effects, 0.42f);
		Test.Settings->SetVolume(EHollowVolume::Voice, -0.2f);
		TestEqual(TEXT("Master at most full"), Test.Settings->GetVolume(EHollowVolume::Master), 1.f, Tolerance);
		TestEqual(TEXT("Effects on a 5 % step"), Test.Settings->GetVolume(EHollowVolume::Effects), 0.4f, Tolerance);
		TestEqual(TEXT("Voice at least silent"), Test.Settings->GetVolume(EHollowVolume::Voice), 0.f, Tolerance);
		TestEqual(TEXT("Every change told"), Changes, 3);
		const UHollowGameUserSettings* Next = Test.Reload();
		TestTrue(TEXT("The next launch has them"), FMath::IsNearlyEqual(Next->GetVolume(EHollowVolume::Effects), 0.4f, Tolerance)
			&& FMath::IsNearlyEqual(Next->GetVolume(EHollowVolume::Voice), 0.f, Tolerance));
	}

	const USoundControlBus* Master = Config->GetBus(EHollowVolume::Master).LoadSynchronous();
	const USoundControlBus* Effects = Config->GetBus(EHollowVolume::Effects).LoadSynchronous();
	const USoundControlBus* Voice = Config->GetBus(EHollowVolume::Voice).LoadSynchronous();
	TestTrue(TEXT("Three volume buses"), IsVolumeBus(Master) && IsVolumeBus(Effects) && IsVolumeBus(Voice));
	if (!Master || !Effects || !Voice)
	{
		return false;
	}
	const USoundClass* World = LoadObject<USoundClass>(nullptr, WorldClass);
	const USoundClass* UI = LoadObject<USoundClass>(nullptr, UIClass);
	const USoundClass* VoiceSounds = LoadObject<USoundClass>(nullptr, VoiceClass);
	const TSet<FName> WorldAndUI = {Master->GetFName(), Effects->GetFName()};
	TestTrue(TEXT("SC_Hollow_World: master and effects"), World && VolumeBuses(World).Num() == 2 && VolumeBuses(World).Includes(WorldAndUI));
	TestTrue(TEXT("SC_Hollow_UI: master and effects"), UI && VolumeBuses(UI).Num() == 2 && VolumeBuses(UI).Includes(WorldAndUI));
	TestTrue(TEXT("SC_Hollow_Voice: master and voice"), VoiceSounds && VolumeBuses(VoiceSounds).Num() == 2
		&& VolumeBuses(VoiceSounds).Includes(TSet<FName>{Master->GetFName(), Voice->GetFName()}));

	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	FARFilter Filter;
	Filter.PackagePaths.Add(AudioRoot);
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Sounds;
	Registry.GetAssets(Filter, Sounds);
	int32 UISounds = 0;
	for (const FAssetData& Asset : Sounds)
	{
		if (Asset.AssetClassPath.GetAssetName() != MetaSoundSourceClass)
		{
			continue;
		}
		const USoundBase* Sound = Cast<USoundBase>(Asset.GetAsset());
		const USoundClass* Class = Sound ? Sound->GetSoundClass() : nullptr;
		const bool bUI = Asset.AssetName.ToString().StartsWith(UISoundPrefix) && Class == UI;
		UISounds += bUI ? 1 : 0;
		TestTrue(FString::Printf(TEXT("%s plays through a class with the player's volumes"), *Asset.AssetName.ToString()),
			Class && (Class == World || Class == UI || Class == VoiceSounds));
	}
	TestEqual(TEXT("The menus' four sounds through SC_Hollow_UI (focus, confirm, deny, terminal)"), UISounds, 4);

	// The subsystem of a game world sets the buses to this machine's settings, and follows them.
	UHollowGameUserSettings* Engine = UHollowGameUserSettings::Get();
	FTestWorldWrapper TestWorld;
	UWorld* Game = HollowGameTests::CreateWorld(*this, TestWorld);
	UHollowAudioVolumeSubsystem* Subsystem = Game ? Game->GetSubsystem<UHollowAudioVolumeSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("The engine's settings"), Engine) || !TestNotNull(TEXT("The volume subsystem of a game world"), Subsystem))
	{
		return false;
	}
	const float KeptEffects = Engine->GetVolume(EHollowVolume::Effects);
	Engine->SetVolume(EHollowVolume::Effects, 0.6f, /*bSave=*/false);
	Subsystem->OnWorldBeginPlay(*Game);
	TestEqual(TEXT("Begun: the effects bus at the setting"), Subsystem->GetAppliedVolume(EHollowVolume::Effects), 0.6f, Tolerance);
	TestEqual(TEXT("...the master bus too"), Subsystem->GetAppliedVolume(EHollowVolume::Master), Engine->GetVolume(EHollowVolume::Master), Tolerance);
	Engine->SetVolume(EHollowVolume::Effects, 0.25f, /*bSave=*/false);
	TestEqual(TEXT("A change follows at once"), Subsystem->GetAppliedVolume(EHollowVolume::Effects), 0.25f, Tolerance);
	Engine->SetVolume(EHollowVolume::Effects, KeptEffects, /*bSave=*/false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSettingsGraphicsTest, "Hollow.Game.Settings.Graphics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSettingsGraphicsTest::RunTest(const FString& Parameters)
{
	// UI-02 (ADR-0052): the frame rate limit is one of 30 / 60 / 120 / none; until the player chooses, the preset's;
	// once chosen it outlives a preset change. The window mode (fullscreen, windowed fullscreen, a window at three
	// quarters of the monitor), the vertical sync and the preset are kept for the next launch.
	using namespace HollowSettingsTests;
	const UHollowPlayerSettingsConfig* Config = GetDefault<UHollowPlayerSettingsConfig>();
	TestTrue(TEXT("Limits 30, 60, 120 and none"), Config->FrameRateLimits == TArray<float>({30.f, 60.f, 120.f, 0.f}));
	TestTrue(TEXT("A window is three quarters of the monitor"), FMath::IsNearlyEqual(Config->WindowedResolutionScale, 0.75f));
	// A preset applies its scalability and frame limit to this editor: put back after.
	const Scalability::FQualityLevels Before = Scalability::GetQualityLevels();
	const float MaxFps = GEngine ? GEngine->GetMaxFPS() : 0.f;
	{
		FTestSettings Test(TEXT("HollowSettingsGraphics"));
		Test.Settings->SetFrameRateChoice(60.f, /*bSave=*/false);
		TestEqual(TEXT("A chosen limit"), Test.Settings->GetFrameRateChoice(), 60.f, Tolerance);
		Test.Settings->ApplyPreset(EHollowGraphicsPreset::Low, /*bSave=*/false);
		TestEqual(TEXT("...outlives a preset change"), Test.Settings->GetFrameRateLimit(), 60.f, Tolerance);
		TestEqual(TEXT("...its preset Low"), Test.Settings->GetPreset(), EHollowGraphicsPreset::Low);
		Test.Settings->SetWindowModeNow(EWindowMode::Windowed, /*bSave=*/false);
		const FIntPoint Desktop = Test.Settings->GetDesktopResolution();
		TestEqual(TEXT("Windowed"), Test.Settings->GetFullscreenMode(), EWindowMode::Windowed);
		if (Desktop.GetMin() > 0)
		{
			TestEqual(TEXT("...three quarters of the monitor wide"), Test.Settings->GetScreenResolution().X, FMath::RoundToInt32(Desktop.X * Config->WindowedResolutionScale));
		}
		Test.Settings->SetVSyncNow(true, /*bSave=*/false);
		TestTrue(TEXT("Vertical sync on"), Test.Settings->IsVSyncEnabled());
		const UHollowGameUserSettings* Next = Test.Reload();
		TestTrue(TEXT("The next launch has the limit, the window, the vertical sync and the preset"), FMath::IsNearlyEqual(Next->GetFrameRateChoice(), 60.f, Tolerance)
			&& Next->GetFullscreenMode() == EWindowMode::Windowed && Next->IsVSyncEnabled() && Next->GetPreset() == EHollowGraphicsPreset::Low);
		Test.Settings->SetFrameRateChoice(0.f, /*bSave=*/false);
		TestEqual(TEXT("None: no limit"), Test.Settings->GetFrameRateChoice(), 0.f, Tolerance);
	}
	Scalability::SetQualityLevels(Before, /*bForce=*/true);
	if (GEngine)
	{
		GEngine->SetMaxFPS(MaxFps);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
