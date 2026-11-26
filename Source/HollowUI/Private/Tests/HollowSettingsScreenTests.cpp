#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Engine/World.h"
#include "Foundation/HollowButton.h"
#include "Foundation/HollowPrimaryLayout.h"
#include "Foundation/HollowUISettings.h"
#include "Foundation/HollowUIStyle.h"
#include "HAL/FileManager.h"
#include "HollowUITags.h"
#include "Hub/HollowTabListWidget.h"
#include "ICommonInputModule.h"
#include "Input/Events.h"
#include "Input/NavigationReply.h"
#include "Localization/HollowLanguage.h"
#include "Menus/HollowSettingRowWidget.h"
#include "Menus/HollowSettingsLayout.h"
#include "Menus/HollowSettingsRules.h"
#include "Menus/HollowSettingsWidget.h"
#include "Misc/Paths.h"
#include "Settings/HollowGameUserSettings.h"
#include "Settings/HollowPlayerSettingsConfig.h"
#include "Sound/SoundBase.h"
#include "Tests/AutomationCommon.h"
#include "Tests/HollowLocalizationTestUtils.h"
#include "Voice/HollowVoiceRules.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace HollowSettingsScreenTests
{
	const TCHAR* const ButtonStyles[] = {
		TEXT("/Game/Hollow/UI/Foundation/BP_ButtonStyle_Menu.BP_ButtonStyle_Menu_C"),
		TEXT("/Game/Hollow/UI/Hub/BP_ButtonStyle_Row.BP_ButtonStyle_Row_C"),
		TEXT("/Game/Hollow/UI/Hub/BP_ButtonStyle_Tab.BP_ButtonStyle_Tab_C"),
		TEXT("/Game/Hollow/UI/Menus/BP_ButtonStyle_Setting.BP_ButtonStyle_Setting_C"),
	};
	const TCHAR* const FocusSound = TEXT("/Game/Hollow/Audio/MetaSounds/MS_UI_Focus.MS_UI_Focus");
	const TCHAR* const ConfirmSound = TEXT("/Game/Hollow/Audio/MetaSounds/MS_UI_Confirm.MS_UI_Confirm");
	const TCHAR* const DenySound = TEXT("/Game/Hollow/Audio/MetaSounds/MS_UI_Deny.MS_UI_Deny");
	const TCHAR* const TerminalSound = TEXT("/Game/Hollow/Audio/MetaSounds/MS_UI_Terminal.MS_UI_Terminal");
	const TCHAR* const MenuButton = TEXT("/Game/Hollow/UI/Foundation/W_MenuButton.W_MenuButton_C");
	const TCHAR* const GameRoot = TEXT("/Game/Hollow");
	const TCHAR* const WaveClasses[] = {TEXT("SoundWave"), TEXT("SoundCue")};
	const FName TabIds[] = {TEXT("Graphics"), TEXT("Audio"), TEXT("Controls"), TEXT("Accessibility")};
	/** The one value every language shows the same: a language's own name (the language row). */
	const TCHAR* const EnglishName = TEXT("English");
	constexpr float Tolerance = 1e-3f;

	/** Settings of the test's own, saving into a file of Saved/Automation (the player's GameUserSettings.ini untouched). */
	struct FTestSettings
	{
		FTestSettings()
			: File(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("HollowSettingsScreen.ini")))
			, Settings(NewObject<UHollowGameUserSettings>(GetTransientPackage()))
		{
			IFileManager::Get().Delete(*File);
			Settings->SetConfigFileForTests(File);
			// A known start, whatever this editor's own settings are.
			Settings->SetVolume(EHollowVolume::Master, 1.f, false);
			Settings->SetMouseSensitivity(1.f, false);
			Settings->SetFieldOfView(90.f, false);
			Settings->SetLookInverted(false, false);
			Settings->SetReducedFlashes(false, false);
			Settings->SetControlsHintShown(true, false);
			Settings->SetFrameRateChoice(0.f, false);
			Settings->SetWaypointMode(EHollowWaypointMode::Full, false);
		}

		~FTestSettings()
		{
			IFileManager::Get().Delete(*File);
		}

		FTestSettings(const FTestSettings&) = delete;
		FTestSettings& operator=(const FTestSettings&) = delete;

		const FString File;
		TStrongObjectPtr<UHollowGameUserSettings> Settings;
	};

	struct FRig
	{
		FTestWorldWrapper TestWorld;
		UHollowPrimaryLayout* Layout = nullptr;
		TSharedPtr<SWidget> LayoutSlate;
		TSubclassOf<UHollowSettingsWidget> SettingsClass;

		bool Build(FAutomationTestBase& Test)
		{
			const UHollowUISettings* UISettings = GetDefault<UHollowUISettings>();
			const TSubclassOf<UHollowPrimaryLayout> LayoutClass = UISettings->PrimaryLayoutClass.LoadSynchronous();
			SettingsClass = UISettings->SettingsScreenClass.LoadSynchronous();
			if (!Test.TestNotNull(TEXT("W_PrimaryLayout"), LayoutClass.Get()) || !Test.TestNotNull(TEXT("W_Settings (UHollowUISettings::SettingsScreenClass)"), SettingsClass.Get())
				|| !TestWorld.CreateTestWorld(EWorldType::Game))
			{
				return false;
			}
			TestWorld.GetTestWorld()->InitializeActorsForPlay(FURL());
			ICommonInputModule::GetSettings().LoadData();
			Layout = CreateWidget<UHollowPrimaryLayout>(TestWorld.GetTestWorld(), LayoutClass);
			if (!Test.TestNotNull(TEXT("Layout"), Layout))
			{
				return false;
			}
			LayoutSlate = Layout->TakeWidget();
			return true;
		}

		UCommonActivatableWidget* Push(const TSoftClassPtr<UCommonActivatableWidget>& Class) const
		{
			return Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Menu, Class.LoadSynchronous());
		}

		UHollowSettingsWidget* OpenSettings(UHollowGameUserSettings* Settings) const
		{
			return Layout->GetLayer(HollowUITags::UI_Layer_Menu)->AddWidget<UHollowSettingsWidget>(SettingsClass, [Settings](UHollowSettingsWidget& Screen)
			{
				Screen.SetSettingsForTests(Settings);
			});
		}

		~FRig()
		{
			if (Layout)
			{
				Layout->RemoveFromParent();
			}
		}
	};

	/** What the row does with a press of the D-pad or an arrow key that way (Slate's own navigation event). */
	FNavigationReply Navigate(UHollowSettingRowWidget& Row, const EUINavigation Way)
	{
		const TSharedRef<SWidget> Slate = Row.TakeWidget();
		const FNavigationEvent Event(FModifierKeysState(), /*InUserIndex=*/0, Way, ENavigationGenesis::Keyboard);
		return Slate->OnNavigation(Row.GetCachedGeometry(), Event);
	}

	bool HasLatinLetter(const FString& Text)
	{
		for (const TCHAR Char : Text)
		{
			if ((Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('A') && Char <= TEXT('Z')))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSettingsScreenTest, "Hollow.UI.Settings.Screen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSettingsScreenTest::RunTest(const FString& Parameters)
{
	// UI-02 (ADR-0052): the real W_Settings over the main menu: four tabs (Graphics, Audio, Controls, Language and
	// accessibility), each row of DA_SettingsLayout steps its setting with left / right (the focus stays on the row),
	// a press or the mouse; a step applies to the player's settings at once and the row shows the new value. Back
	// returns to the menu that opened it: the main menu, the system menu.
	using namespace HollowSettingsScreenTests;
	FRig Rig;
	FTestSettings Test;
	UHollowGameUserSettings& Settings = *Test.Settings;
	if (!Rig.Build(*this))
	{
		return false;
	}
	const UHollowUISettings* UISettings = GetDefault<UHollowUISettings>();
	UCommonActivatableWidget* Main = Rig.Push(UISettings->MainMenuClass);
	UHollowSettingsWidget* Screen = Rig.OpenSettings(&Settings);
	if (!TestNotNull(TEXT("Main menu"), Main) || !TestNotNull(TEXT("W_Settings on the Menu layer"), Screen) || !TestNotNull(TEXT("Its layout"), Screen->GetLayout()))
	{
		return false;
	}
	const TOptional<FUIInputConfig> Input = Screen->GetDesiredInputConfig();
	TestTrue(TEXT("Menu input"), Input.IsSet() && Input->GetInputMode() == ECommonInputMode::Menu);
	TestFalse(TEXT("The main menu waits under it"), Main->IsActivated());
	TestEqual(TEXT("Four tabs"), Screen->GetTabs()->GetTabCount(), static_cast<int32>(UE_ARRAY_COUNT(TabIds)));
	TestEqual(TEXT("Graphics first"), Screen->GetSelectedTab(), TabIds[0]);
	TArray<UHollowSettingRowWidget*> Rows = Screen->GetRowWidgets();
	TestTrue(TEXT("Graphics: preset, display mode, vertical sync, frame rate limit"), Rows.Num() == 4 && Rows[0]->GetSetting() == EHollowSettingId::GraphicsPreset
		&& Rows[1]->GetSetting() == EHollowSettingId::WindowMode && Rows[2]->GetSetting() == EHollowSettingId::VSync && Rows[3]->GetSetting() == EHollowSettingId::FrameRateLimit);
	TestTrue(TEXT("The gamepad starts on the first row"), !Rows.IsEmpty() && Screen->GetDesiredFocusTarget() == Rows[0]);
	const FHollowSettingsTexts& Texts = Screen->GetLayout()->Texts;
	UHollowSettingRowWidget* Row = Screen->FindRow(EHollowSettingId::FrameRateLimit);
	TestTrue(TEXT("No limit chosen: Unlimited"), Row && Row->GetValue().EqualTo(Texts.Unlimited));
	Row->Step(1);
	TestEqual(TEXT("The next limit wraps round to 30"), Settings.GetFrameRateChoice(), 30.f, Tolerance);
	TestTrue(TEXT("...the row says so"), Row->GetValue().ToString().Contains(TEXT("30")));
	Row = Screen->FindRow(EHollowSettingId::VSync);
	const bool bVSync = Settings.IsVSyncEnabled();
	Row->Step(1);
	TestTrue(TEXT("Vertical sync toggled, shown"), Settings.IsVSyncEnabled() != bVSync && Row->GetValue().EqualTo(bVSync ? Texts.Off : Texts.On));
	Row = Screen->FindRow(EHollowSettingId::WindowMode);
	const EWindowMode::Type Mode = Settings.GetFullscreenMode();
	Row->Step(1);
	TestTrue(TEXT("Display mode: the next one"), Settings.GetFullscreenMode() != Mode);

	Screen->SelectTab(TabIds[1]);
	Rows = Screen->GetRowWidgets();
	TestTrue(TEXT("Audio: master, effects, voice, microphone"), Screen->GetSelectedTab() == TabIds[1] && Rows.Num() == 4
		&& Rows[0]->GetSetting() == EHollowSettingId::MasterVolume && Rows[3]->GetSetting() == EHollowSettingId::VoiceMode);
	Row = Screen->FindRow(EHollowSettingId::MasterVolume);
	const FNavigationReply Left = Navigate(*Row, EUINavigation::Left);
	FNumberFormattingOptions Whole;
	Whole.MaximumFractionalDigits = 0;
	TestTrue(TEXT("Left: the master volume a step down, the focus stays on the row"), Left.GetBoundaryRule() == EUINavigationRule::Stop
		&& FMath::IsNearlyEqual(Settings.GetVolume(EHollowVolume::Master), 0.95f, Tolerance) && Row->GetValue().EqualTo(FText::AsPercent(0.95f, &Whole)));
	Navigate(*Row, EUINavigation::Right);
	Navigate(*Row, EUINavigation::Right);
	TestEqual(TEXT("Right twice: full, and no further"), Settings.GetVolume(EHollowVolume::Master), 1.f, Tolerance);
	TestTrue(TEXT("Down is the list's: to the next row"), Navigate(*Row, EUINavigation::Down).GetBoundaryRule() != EUINavigationRule::Stop);
	Row = Screen->FindRow(EHollowSettingId::VoiceMode);
	Settings.SetVoiceMode(EHollowVoiceMode::PushToTalk);
	Row->Step(1);
	TestTrue(TEXT("Microphone: push-to-talk, then open mic"), Settings.GetVoiceMode() == EHollowVoiceMode::OpenMic && Row->GetValue().EqualTo(Texts.OpenMic));

	Screen->SelectTab(TabIds[2]);
	Row = Screen->FindRow(EHollowSettingId::MouseSensitivity);
	Navigate(*Row, EUINavigation::Right);
	TestEqual(TEXT("Controls: the sensitivity a step up"), Settings.GetMouseSensitivity(), 1.1f, Tolerance);
	Row = Screen->FindRow(EHollowSettingId::FieldOfView);
	Row->Step(1);
	TestEqual(TEXT("...the field of view a degree wider"), Settings.GetFieldOfView(), 91.f, Tolerance);
	TestTrue(TEXT("...shown in degrees"), Row->GetValue().ToString().Contains(TEXT("91")));
	Screen->FindRow(EHollowSettingId::InvertLook)->Step(1);
	Screen->FindRow(EHollowSettingId::ControlsHint)->Step(1);
	TestTrue(TEXT("...the look inverted, the cheat sheet folded"), Settings.IsLookInverted() && !Settings.IsControlsHintShown());

	Screen->SelectTab(TabIds[3]);
	Row = Screen->FindRow(EHollowSettingId::ReducedFlashes);
	Row->Step(1);
	TestTrue(TEXT("Accessibility: reduced flashes on"), Settings.IsReducedFlashes() && Row->GetValue().EqualTo(Texts.On));
	Row = Screen->FindRow(EHollowSettingId::Waypoints);
	TestTrue(TEXT("POLISH-01: the waypoints row, full"), Row && Row->GetValue().EqualTo(Texts.WaypointsFull) && !Texts.WaypointsImmersive.IsEmpty());
	if (Row)
	{
		Navigate(*Row, EUINavigation::Right);
		TestTrue(TEXT("...a step: immersive, kept in the settings"), Settings.GetWaypointMode() == EHollowWaypointMode::Immersive
			&& Row->GetValue().EqualTo(Texts.WaypointsImmersive));
		Row->Step(-1);
		TestEqual(TEXT("...and back: full"), Settings.GetWaypointMode(), EHollowWaypointMode::Full);
	}
	Row = Screen->FindRow(EHollowSettingId::Language);
	TestEqual(TEXT("The language row names English in English"), Row ? Row->GetValue().ToString() : FString(), FString(EnglishName));
	Row->Step(1);
	TestTrue(TEXT("A step: Russian, at once"), HollowLanguage::GetApplied() == EHollowLanguage::Russian);
	TestTrue(FString::Printf(TEXT("...the row reads in Russian (\"%s\")"), *Row->GetLabel().ToString()), HollowLocalizationTests::HasCyrillic(Row->GetLabel().ToString()));
	Row->Step(1);
	TestTrue(TEXT("Another: English again"), HollowLanguage::GetApplied() == EHollowLanguage::English);
	HollowLanguage::Apply(EHollowLanguage::English);

	TestTrue(TEXT("Back is handled"), Screen->NativeOnHandleBackAction());
	TestTrue(TEXT("...the main menu is back"), !Screen->IsActivated() && Main->IsActivated());
	Main->DeactivateWidget();

	UCommonActivatableWidget* System = Rig.Push(UISettings->SystemMenuClass);
	Screen = Rig.OpenSettings(&Settings);
	TestTrue(TEXT("From the system menu too"), System && Screen && !System->IsActivated());
	if (Screen)
	{
		Screen->NativeOnHandleBackAction();
	}
	TestTrue(TEXT("...and back to it"), System && System->IsActivated());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLocalizationSettingsTest, "Hollow.UI.Localization.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLocalizationSettingsTest::RunTest(const FString& Parameters)
{
	// UI-02 + LOC-01 (ADR-0048, ADR-0052): every tab of the settings screen in Russian: every text with letters reads
	// in Russian (the language row names English in English: a language's own name), nothing is cut on a 1080p screen.
	using namespace HollowSettingsScreenTests;
	using namespace HollowLocalizationTests;
	FRig Rig;
	FTestSettings Test;
	if (!TestTrue(TEXT("Slate measures text here"), CanMeasureText()) || !Rig.Build(*this))
	{
		return false;
	}
	UHollowSettingsWidget* Screen = Rig.OpenSettings(Test.Settings.Get());
	if (!TestNotNull(TEXT("W_Settings"), Screen))
	{
		return false;
	}
	const FVector2D Size(1920.0, 1080.0);
	const FScopedLanguage Russian(EHollowLanguage::Russian);
	TestTrue(TEXT("Russian applied"), Russian.bApplied);
	for (const FName& Tab : TabIds)
	{
		Screen->SelectTab(Tab);
		for (const TPair<FString, FString>& Text : ReadTexts(*Screen))
		{
			TestTrue(FString::Printf(TEXT("%s, %s: \"%s\" in Russian"), *Tab.ToString(), *Text.Key, *Text.Value),
				!HasLatinLetter(Text.Value) || HasCyrillic(Text.Value) || Text.Value == EnglishName);
		}
		for (const FString& Overflow : FindOverflows(*Screen, Size))
		{
			AddError(FString::Printf(TEXT("%s: cut in Russian: %s"), *Tab.ToString(), *Overflow));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowUISoundsTest, "Hollow.UI.Sounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowUISoundsTest::RunTest(const FString& Parameters)
{
	// UI-02 (ADR-0052): every button style of the menus and hub screens plays the UI's MetaSounds: the focus (the
	// mouse's hover; the keys' focus through UHollowButton), the confirm on a press, the deny on a refused press (a
	// locked button: greyed out by DA_UIStyle, still focusable); the terminal screens open and close with theirs. The
	// game holds no WAV: every sound is a procedural MetaSound.
	using namespace HollowSettingsScreenTests;
	const USoundBase* Focus = LoadObject<USoundBase>(nullptr, FocusSound);
	const USoundBase* Confirm = LoadObject<USoundBase>(nullptr, ConfirmSound);
	const USoundBase* Deny = LoadObject<USoundBase>(nullptr, DenySound);
	if (!TestTrue(TEXT("MS_UI_Focus, MS_UI_Confirm, MS_UI_Deny"), Focus && Confirm && Deny))
	{
		return false;
	}
	for (const TCHAR* Path : ButtonStyles)
	{
		const UClass* Class = LoadClass<UCommonButtonStyle>(nullptr, Path);
		const UCommonButtonStyle* Style = Class ? Class->GetDefaultObject<UCommonButtonStyle>() : nullptr;
		if (!TestNotNull(Path, Style))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s: focus, confirm, deny"), *FPaths::GetBaseFilename(Path)), Style->HoveredSlateSound.GetResourceObject() == Focus
			&& Style->PressedSlateSound.GetResourceObject() == Confirm && Style->LockedPressedSlateSound.bHasSound
			&& Style->LockedPressedSlateSound.Sound.GetResourceObject() == Deny);
	}
	const UHollowUIStyle* UIStyle = UHollowUIStyle::Get();
	if (!TestNotNull(TEXT("DA_UIStyle"), UIStyle))
	{
		return false;
	}
	TestTrue(TEXT("The terminal's sound, pitched down on closing"), UIStyle->TerminalSound.ToSoftObjectPath().ToString() == TerminalSound
		&& UIStyle->TerminalClosePitch < 1.f);
	TestTrue(TEXT("Three badge kinds, their widget"), UIStyle->Badges.Num() == 3 && !UIStyle->BadgeClass.IsNull()
		&& UIStyle->FindBadge(EHollowHubBadgeKind::Price) && UIStyle->FindBadge(EHollowHubBadgeKind::Slot) && UIStyle->FindBadge(EHollowHubBadgeKind::Insured));

	// A refused button: locked (its press plays the deny sound, tells OnLockClicked), greyed out, then allowed again.
	FTestWorldWrapper TestWorld;
	const TSubclassOf<UHollowButton> ButtonClass = LoadClass<UHollowButton>(nullptr, MenuButton);
	if (!TestNotNull(TEXT("W_MenuButton"), ButtonClass.Get()) || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		return false;
	}
	UHollowButton* Button = CreateWidget<UHollowButton>(TestWorld.GetTestWorld(), ButtonClass);
	Button->SetRefused(true);
	TestTrue(TEXT("Refused: locked, greyed out"), Button->IsRefused() && Button->GetLocked() && FMath::IsNearlyEqual(Button->GetRenderOpacity(), UIStyle->RefusedOpacity));
	TestTrue(TEXT("...still interactable (focus, the deny sound, its reason)"), Button->GetIsEnabled() && Button->IsInteractionEnabled());
	Button->SetRefused(false);
	TestTrue(TEXT("Allowed again"), !Button->IsRefused() && FMath::IsNearlyEqual(Button->GetRenderOpacity(), 1.f));

	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	FARFilter Filter;
	Filter.PackagePaths.Add(GameRoot);
	Filter.bRecursivePaths = true;
	for (const TCHAR* Class : WaveClasses)
	{
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), Class));
	}
	TArray<FAssetData> Waves;
	Registry.GetAssets(Filter, Waves);
	for (const FAssetData& Wave : Waves)
	{
		AddError(FString::Printf(TEXT("%s is a %s: the game's sounds are MetaSounds"), *Wave.PackageName.ToString(), *Wave.AssetClassPath.GetAssetName().ToString()));
	}
	TestEqual(TEXT("0 WAV in /Game/Hollow"), Waves.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
