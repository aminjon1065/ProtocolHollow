#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CommonButtonBase.h"
#include "CommonInputSettings.h"
#include "Engine/World.h"
#include "Foundation/HollowPrimaryLayout.h"
#include "Foundation/HollowUISettings.h"
#include "HollowUITags.h"
#include "ICommonInputModule.h"
#include "Menus/HollowMainMenuWidget.h"
#include "Menus/HollowMenuRules.h"
#include "Menus/HollowSystemMenuWidget.h"
#include "Raid/HollowDebriefWidget.h"
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
	// The real WBPs (create_ui_assets.py): the main menu takes menu input with focus on Host co-op and binds its three
	// buttons; the system menu has the session buttons and shows those that apply (none in a test world: no session);
	// Back closes a debrief.
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
		TestTrue(TEXT("...solo, host co-op and quit bound"), Main->GetSoloButton() && Main->GetHostButton() && Main->GetQuitButton());
		TestTrue(TEXT("...gamepad focus on Host co-op"), Main->GetDesiredFocusTarget() == Main->GetHostButton());
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
		// H-1 (ADR-0029): the reduced-flashes toggle, shown wherever the menu is, labelled with the choice.
		TestTrue(TEXT("...has the flashes toggle, shown"), System->GetFlashesButton() && Shown(System->GetFlashesButton()));
		TestFalse(TEXT("...its two labels differ"), UHollowSystemMenuWidget::FlashesLabel(true).EqualTo(UHollowSystemMenuWidget::FlashesLabel(false)));
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

#endif // WITH_DEV_AUTOMATION_TESTS
