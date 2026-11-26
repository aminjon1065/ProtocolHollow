#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "GeneralProjectSettings.h"
#include "CommonInputSettings.h"
#include "Engine/World.h"
#include "Foundation/HollowButton.h"
#include "Foundation/HollowPrimaryLayout.h"
#include "Foundation/HollowUISettings.h"
#include "HollowUITags.h"
#include "ICommonInputModule.h"
#include "Menus/HollowMainMenuWidget.h"
#include "Menus/HollowMenuRules.h"
#include "Menus/HollowSystemMenuWidget.h"
#include "Raid/HollowDebriefWidget.h"
#include "Tests/HollowLocalizationTestUtils.h"
#include "Tests/AutomationCommon.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace HollowMenuTests
{
	FHollowMenuContext Context(const bool bInSession, const bool bHost, const bool bOnline, const bool bInHub)
	{
		FHollowMenuContext Out;
		Out.bInSession = bInSession;
		Out.bHost = bHost;
		Out.bOnline = bOnline;
		Out.bInHub = bInHub;
		return Out;
	}

	bool Shown(const UCommonButtonBase* Button)
	{
		return Button && Button->GetVisibility() != ESlateVisibility::Collapsed;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowMenuRulesTest, "Hollow.UI.Menus.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowMenuRulesTest::RunTest(const FString& Parameters)
{
	// The system menu's session buttons (S4-06): invite only when hosting online, start a raid only as the host in the hub,
	// leave whenever in a session; nothing outside one.
	using namespace HollowMenuTests;
	struct FCase
	{
		const TCHAR* What;
		FHollowMenuContext In;
		bool bInvite;
		bool bStartRaid;
		bool bLeave;
	};
	const FCase Cases[] = {
		{TEXT("No session (front end)"), Context(false, true, false, false), false, false, false},
		{TEXT("Solo host in the hub"), Context(true, true, false, true), false, true, true},
		{TEXT("Co-op host in the hub"), Context(true, true, true, true), true, true, true},
		{TEXT("Co-op host in a raid"), Context(true, true, true, false), true, false, true},
		{TEXT("Client in the hub"), Context(true, false, true, true), false, false, true},
	};
	for (const FCase& Case : Cases)
	{
		const FHollowSystemMenuOptions Options = HollowMenuRules::SystemMenuOptions(Case.In);
		TestTrue(FString::Printf(TEXT("%s: invite %d, start raid %d, leave %d"), Case.What, Case.bInvite, Case.bStartRaid, Case.bLeave),
			Options.bInvite == Case.bInvite && Options.bStartRaid == Case.bStartRaid && Options.bLeave == Case.bLeave);
	}
	TestFalse(TEXT("No world: not the front end"), HollowMenuRules::IsFrontEnd(nullptr));
	const FHollowMenuContext Nowhere = HollowMenuRules::ReadContext(nullptr);
	TestFalse(TEXT("No world: no session, no hub"), Nowhere.bInSession || Nowhere.bInHub);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowMenuWidgetsTest, "Hollow.UI.Menus.Widgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowMenuWidgetsTest::RunTest(const FString& Parameters)
{
	// The real WBPs (create_menu_ui_assets.py): the main menu takes menu input with focus on Host co-op and binds its four
	// buttons and its version line (UI-02); the system menu has the session buttons and shows those
	// that apply (none in a test world: no session), opens the settings and has no toggles of its own any more (they
	// moved to the settings screen); Back closes a debrief.
	using namespace HollowMenuTests;
	const UHollowUISettings* Settings = GetDefault<UHollowUISettings>();
	const TSubclassOf<UHollowPrimaryLayout> LayoutClass = Settings->PrimaryLayoutClass.LoadSynchronous();
	const TSubclassOf<UCommonActivatableWidget> MainClass = Settings->MainMenuClass.LoadSynchronous();
	const TSubclassOf<UCommonActivatableWidget> SystemClass = Settings->SystemMenuClass.LoadSynchronous();
	const TSubclassOf<UHollowDebriefWidget> DebriefClass = Settings->DebriefClass.LoadSynchronous();
	FTestWorldWrapper TestWorld;
	if (!TestNotNull(TEXT("W_PrimaryLayout"), LayoutClass.Get()) || !TestNotNull(TEXT("W_MainMenu (UHollowUISettings::MainMenuClass)"), MainClass.Get())
		|| !TestNotNull(TEXT("W_SystemMenu"), SystemClass.Get()) || !TestNotNull(TEXT("W_Debrief"), DebriefClass.Get()) || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	ICommonInputModule::GetSettings().LoadData();
	UHollowPrimaryLayout* Layout = CreateWidget<UHollowPrimaryLayout>(World, LayoutClass);
	if (!TestNotNull(TEXT("Layout"), Layout))
	{
		return false;
	}
	const TSharedRef<SWidget> LayoutSlate = Layout->TakeWidget();
	TestFalse(TEXT("A test world is not the front end"), HollowMenuRules::IsFrontEnd(World));

	UHollowMainMenuWidget* Main = Cast<UHollowMainMenuWidget>(Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Menu, MainClass));
	if (TestNotNull(TEXT("Main menu on the Menu layer"), Main))
	{
		const TOptional<FUIInputConfig> Input = Main->GetDesiredInputConfig();
		TestTrue(TEXT("...menu input"), Input.IsSet() && Input->GetInputMode() == ECommonInputMode::Menu);
		TestTrue(TEXT("...solo, host co-op, settings and quit bound"), Main->GetSoloButton() && Main->GetHostButton() && Main->GetSettingsButton() && Main->GetQuitButton());
		TestTrue(TEXT("...gamepad focus on Host co-op"), Main->GetDesiredFocusTarget() == Main->GetHostButton());
		TestTrue(TEXT("...its version line shows the project's version"), Main->GetVersionText()
			&& Main->GetVersionText()->GetText().ToString().Contains(GetDefault<UGeneralProjectSettings>()->ProjectVersion));
		Main->DeactivateWidget();
	}

	UHollowSystemMenuWidget* System = Cast<UHollowSystemMenuWidget>(Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Menu, SystemClass));
	if (TestNotNull(TEXT("System menu"), System))
	{
		TestTrue(TEXT("...has the session buttons"), System->GetInviteButton() && System->GetStartRaidButton() && System->GetLeaveButton());
		TestFalse(TEXT("Opened without a session: no session buttons"), Shown(System->GetInviteButton()) || Shown(System->GetStartRaidButton()) || Shown(System->GetLeaveButton()));
		System->ApplyContext(Context(true, true, true, true));
		TestTrue(TEXT("Co-op host in the hub: all three"), Shown(System->GetInviteButton()) && Shown(System->GetStartRaidButton()) && Shown(System->GetLeaveButton()));
		System->ApplyContext(Context(true, false, true, true));
		TestTrue(TEXT("Client: only Leave"), !Shown(System->GetInviteButton()) && !Shown(System->GetStartRaidButton()) && Shown(System->GetLeaveButton()));
		// UI-02: Settings, shown wherever the menu is; voice, flashes and language moved there.
		TestTrue(TEXT("...has Settings, shown"), System->GetSettingsButton() && Shown(System->GetSettingsButton()));
		for (const TCHAR* Gone : {TEXT("VoiceButton"), TEXT("FlashesButton"), TEXT("LanguageButton")})
		{
			TestNull(FString::Printf(TEXT("...no %s of its own"), Gone), System->WidgetTree->FindWidget(Gone));
		}
		System->DeactivateWidget();
	}
	// Bug bash: Back (Esc / B) closes a debrief like its button (it was ignored).
	UHollowDebriefWidget* Debrief = Cast<UHollowDebriefWidget>(Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Menu, DebriefClass));
	if (TestNotNull(TEXT("Debrief"), Debrief))
	{
		TestTrue(TEXT("Back is handled by the debrief"), Debrief->NativeOnHandleBackAction());
		TestFalse(TEXT("...and closes it"), Debrief->IsActivated());
	}
	Layout->RemoveFromParent();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLocalizationMenusTest, "Hollow.UI.Localization.Menus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLocalizationMenusTest::RunTest(const FString& Parameters)
{
	// LOC-01 (ADR-0048): switching shows an open menu in Russian at once (the same widgets, no restart), nothing is cut
	// on a 1080p screen, and English comes back the same. The main menu's language button names the new language in it
	// (UI-02: the system menu's moved to the settings screen). The logo and the version line stay the same.
	using namespace HollowMenuTests;
	using namespace HollowLocalizationTests;
	const FVector2D Screen(1920.0, 1080.0);
	const UHollowUISettings* Settings = GetDefault<UHollowUISettings>();
	const TSubclassOf<UHollowPrimaryLayout> LayoutClass = Settings->PrimaryLayoutClass.LoadSynchronous();
	const TSubclassOf<UCommonActivatableWidget> MainClass = Settings->MainMenuClass.LoadSynchronous();
	const TSubclassOf<UCommonActivatableWidget> SystemClass = Settings->SystemMenuClass.LoadSynchronous();
	FTestWorldWrapper TestWorld;
	if (!TestTrue(TEXT("Slate measures text here"), CanMeasureText()) || !LayoutClass || !MainClass || !SystemClass || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	ICommonInputModule::GetSettings().LoadData();
	UHollowPrimaryLayout* Layout = CreateWidget<UHollowPrimaryLayout>(World, LayoutClass);
	if (!TestNotNull(TEXT("Layout"), Layout))
	{
		return false;
	}
	Layout->TakeWidget();

	for (const TSubclassOf<UCommonActivatableWidget>& Class : {MainClass, SystemClass})
	{
		UCommonActivatableWidget* Menu = Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Menu, Class);
		if (!TestNotNull(*Class->GetName(), Menu))
		{
			continue;
		}
		const UHollowButton* Language = Cast<UHollowButton>(Menu->WidgetTree->FindWidget(TEXT("LanguageButton")));
		if (Class == MainClass && !TestNotNull(TEXT("The main menu has the language button"), Language))
		{
			continue;
		}
		TestTrue(TEXT("...nothing cut in English"), FindOverflows(*Menu, Screen).IsEmpty());   // lays it out: every label is set
		const TMap<FString, FString> English = ReadTexts(*Menu);
		if (Language)
		{
			TestEqual(TEXT("...labelled with English"), Language->GetLabel().ToString(), FString(TEXT("Language: English")));
		}
		{
			const FScopedLanguage Russian(EHollowLanguage::Russian);
			TestTrue(TEXT("Russian applied"), Russian.bApplied && HollowLanguage::GetApplied() == EHollowLanguage::Russian);
			if (Language)
			{
				TestTrue(FString::Printf(TEXT("...the button names Russian, in Russian (\"%s\")"), *Language->GetLabel().ToString()),
					HasCyrillic(Language->GetLabel().ToString()));
			}
			const TMap<FString, FString> Now = ReadTexts(*Menu);
			const FString Version = UHollowMainMenuWidget::VersionLabel().ToString();
			for (const TPair<FString, FString>& Text : English)
			{
				const FString* Shown = Now.Find(Text.Key);
				const bool bSame = Text.Value == TEXT("PROTOCOL: HOLLOW") || (Shown && *Shown == Version);
				TestTrue(FString::Printf(TEXT("%s: \"%s\" -> \"%s\""), *Text.Key, *Text.Value, Shown ? **Shown : TEXT("-")),
					Shown && (bSame ? *Shown == Text.Value : HasCyrillic(*Shown)));
			}
			for (const FString& Overflow : FindOverflows(*Menu, Screen))
			{
				AddError(TEXT("Cut in Russian: ") + Overflow);
			}
		}
		for (const FString& Change : Differences(English, ReadTexts(*Menu)))
		{
			AddError(TEXT("Not back in English: ") + Change);
		}
		Menu->DeactivateWidget();
	}
	Layout->RemoveFromParent();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
