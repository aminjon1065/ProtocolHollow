// Development console commands for the visor and menus (headless and multi-process checks). Never shipped.
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Classes/HollowClassDefinition.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Foundation/HollowHUD.h"
#include "Foundation/HollowPrimaryLayout.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowUILog.h"
#include "HollowUITags.h"
#include "Hub/HollowContractTerminalWidget.h"
#include "Hub/HollowHubRowWidget.h"
#include "Hub/HollowHubScreenRouter.h"
#include "Hub/HollowHubScreenWidget.h"
#include "Hub/HollowWorkbenchWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Localization/HollowLanguage.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Util/HollowEnumUtils.h"
#include "ViewModels/HollowObjectiveModel.h"
#include "Profile/HollowProfileSubsystem.h"
#include "ViewModels/HollowPromptModel.h"
#include "ViewModels/HollowRadioModel.h"
#include "ViewModels/HollowToolTargetModel.h"
#include "ViewModels/HollowVisorContext.h"
#include "Voice/HollowVoiceComponent.h"
#include "Raid/HollowRaidStatusWidget.h"
#include "Settings/HollowGameUserSettings.h"
#include "Visor/HollowStatBarWidget.h"
#include "Visor/HollowVisorElement.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#if !UE_BUILD_SHIPPING

namespace HollowUIDevConsole
{
	APlayerController* LocalPlayer(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	}

	/** The hub screen shown (S5-04): tab, rows with their buttons (- greyed out, > focused), status and message. */
	void DumpHubScreen(const UHollowHubScreenWidget* Screen, APlayerController* Player)
	{
		if (!Screen)
		{
			return;
		}
		const UHollowWorkbenchWidget* Workbench = Cast<UHollowWorkbenchWidget>(Screen);
		const UHollowContractTerminalWidget* Contracts = Cast<UHollowContractTerminalWidget>(Screen);
		const UCommonButtonBase* Launch = Contracts ? Contracts->GetLaunchButton() : nullptr;
		const FString Tab = Workbench ? TEXT(" tab ") + HollowEnumToString(Workbench->GetTab()) : FString();
		const FString LaunchState = Launch ? (Launch->IsVisible() ? FString(TEXT(" launch shown")) : FString(TEXT(" launch hidden"))) : FString();
		UCommonButtonBase* Close = Screen->GetCloseButton();
		const int32 bCloseFocused = Close->HasUserFocus(Player) || Close->HasUserFocusedDescendants(Player) ? 1 : 0;
		const TArray<UHollowHubRowWidget*> Rows = Screen->GetRowWidgets();
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: hub screen %s%s%s, %d row(s), close focus %d, status \"%s\", message \"%s\""), *Screen->GetName(),
			*Tab, *LaunchState, Rows.Num(), bCloseFocused, *Screen->GetStatus().ToString(), *Screen->GetMessage().ToString());
		if (Contracts)
		{
			UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: hub role \"%s\""), *Contracts->GetRoleText().ToString().Replace(TEXT("\n"), TEXT(" | ")));   // one log line
		}
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const FHollowHubRow& Row = Rows[Index]->GetRow();
			const int32 Focused = Rows[Index]->GetFocusedAction(Player);
			FString Buttons;
			for (int32 Action = 0; Action < Row.Actions.Num(); ++Action)
			{
				Buttons += FString::Printf(TEXT(" %s%s[%s]"), Action == Focused ? TEXT(">") : TEXT(""), Row.Actions[Action].bAvailable ? TEXT("") : TEXT("-"),
					*Row.Actions[Action].Label.ToString());
			}
			const TCHAR* Kind = Row.bHeader ? TEXT(" header") : Row.bHighlighted ? TEXT(" *") : TEXT("");
			UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: hub row %d%s \"%s\" (%s)%s"), Index, Kind, *Row.Name.ToString(), *Row.Detail.ToString(), *Buttons);
		}
	}

	/** Prints this machine's UI: layers, input mode and device, visor bars, prompt, tool target, menu focus. */
	void Dump(UWorld* World)
	{
		APlayerController* Player = LocalPlayer(World);
		const AHollowHUD* HUD = AHollowHUD::Get(Player);
		const UHollowPrimaryLayout* Layout = HUD ? HUD->GetPrimaryLayout() : nullptr;
		const UHollowVisorContext* Context = HUD ? HUD->GetVisorContext() : nullptr;
		if (!Layout || !Context)
		{
			UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: no Hollow UI on this machine."));
			return;
		}
		const FGameplayTag LayerTags[] = {HollowUITags::UI_Layer_Game, HollowUITags::UI_Layer_GameMenu, HollowUITags::UI_Layer_Menu, HollowUITags::UI_Layer_Modal};
		for (const FGameplayTag& Tag : LayerTags)
		{
			const UCommonActivatableWidgetContainerBase* Layer = Layout->GetLayer(Tag);
			const UCommonActivatableWidget* Active = Layer ? Layer->GetActiveWidget() : nullptr;
			UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: layer %s: %d widget(s), active %s (activated %d)"), *Tag.ToString(),
				Layer ? Layer->GetNumWidgets() : 0, *GetNameSafe(Active), Active ? Active->IsActivated() : 0);
		}
		const UCommonUIActionRouterBase* Router = ULocalPlayer::GetSubsystem<UCommonUIActionRouterBase>(Player->GetLocalPlayer());
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: input mode %s, device %s, view %s"), Router ? *HollowEnumToString(Router->GetActiveInputMode()) : TEXT("?"),
			*HollowEnumToString(Context->GetInputType()), *Player->GetControlRotation().ToCompactString());
		if (const APawn* Pawn = Player->GetPawn())
		{
			UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: pawn at %s"), *Pawn->GetActorLocation().ToCompactString());
		}
		if (const UHollowVoiceComponent* Voice = Context->GetVoice())
		{
			UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: voice %s"), *Voice->Describe());
		}
		const UHollowClassDefinition* Class = Context->GetClass();
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: class %s, telemetry colour #%s"), Class ? *Class->GetPrimaryAssetId().PrimaryAssetName.ToString() : TEXT("none"),
			*Context->GetTelemetryColor().ToFColor(true).ToHex());   // S6-01: the class's visor colour
		// Where Slate believes the game window and the cursor are: a click lands only inside that window (S4-06: the
		// windowed-fullscreen Mac window lost every click).
		if (const UGameViewportClient* Viewport = World->GetGameViewport())
		{
			const TSharedPtr<SWindow> Window = Viewport->GetWindow();
			const FSlateApplication& Slate = FSlateApplication::Get();
			UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: window at %s size %s, mode %d (0 fullscreen, 1 windowed fullscreen, 2 windowed), cursor %s (inside %d), DPI scale %.2f"),
				Window ? *Window->GetPositionInScreen().ToString() : TEXT("-"), Window ? *Window->GetSizeInScreen().ToString() : TEXT("-"),
				Window ? static_cast<int32>(Window->GetWindowMode()) : -1, *Slate.GetCursorPos().ToString(),
				Window ? Window->IsScreenspaceMouseWithin(Slate.GetCursorPos()) : 0, Window ? Window->GetDPIScaleFactor() : 0.f);
		}

		const UCommonActivatableWidgetContainerBase* GameLayer = Layout->GetLayer(HollowUITags::UI_Layer_Game);
		const UUserWidget* Visor = GameLayer ? GameLayer->GetActiveWidget() : nullptr;
		if (Visor && Visor->WidgetTree)
		{
			Visor->WidgetTree->ForEachWidget([](UWidget* Widget)
			{
				if (const UHollowStatBarWidget* Bar = Cast<UHollowStatBarWidget>(Widget))
				{
					UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: bar %s %.1f / %.1f alert %d"), *Bar->GetLabel().ToString(), Bar->GetValue(), Bar->GetMax(), Bar->IsAlert());
				}
				else if (const UHollowVisorElement* Element = Cast<UHollowVisorElement>(Widget))
				{
					const FString Line = Element->Describe();
					UE_CLOG(!Line.IsEmpty(), LogHollowUI, Display, TEXT("Hollow.UI.Dump: %s"), *Line);
				}
				else if (const UHollowRaidStatusWidget* Raid = Cast<UHollowRaidStatusWidget>(Widget))
				{
					UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: raid line \"%s\""), *Raid->GetStatus().ToString());
				}
			});
		}
		if (const UHollowObjectiveModel* Objective = Context->GetObjective())
		{
			UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: %s"), *Objective->Describe());   // UX-01, also when the tracker is hidden
		}
		if (const UHollowRadioModel* Radio = Context->GetRadio())
		{
			UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: %s"), *Radio->Describe());   // NAR-01a
		}
		const UHollowPromptModel* Prompt = Context->GetPrompt();
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: prompt %d [%s] %s (%s)"), Prompt->IsVisible(), *Prompt->GetKeyLabel().ToString(),
			*Prompt->GetPrompt().ToString(), *GetNameSafe(Prompt->GetFocus()));
		const UHollowToolTargetModel* Target = Context->GetToolTarget();
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: target %d %s %.3f marker %.2f (%s)"), Target->IsVisible(), *Target->GetActionLabel().ToString(),
			Target->GetStatus().Value, Target->GetStatus().Marker, *GetNameSafe(Target->GetTarget()));

		const UCommonActivatableWidgetContainerBase* MenuLayer = Layout->GetLayer(HollowUITags::UI_Layer_Menu);
		const UCommonActivatableWidget* Menu = MenuLayer ? MenuLayer->GetActiveWidget() : nullptr;
		const UWidget* Focus = Menu ? Menu->GetDesiredFocusTarget() : nullptr;
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Dump: menu %s, focus target %s has focus %d"), *GetNameSafe(Menu), *GetNameSafe(Focus),
			Focus ? (Focus->HasUserFocus(Player) || Focus->HasUserFocusedDescendants(Player)) : 0);   // buttons focus their inner SButton
		DumpHubScreen(HUD->GetHubScreens() ? HUD->GetHubScreens()->GetScreen() : nullptr, Player);
	}

	/** Usage: Hollow.UI.Menu <Open|Close> */
	void Menu(const TArray<FString>& Args, UWorld* World)
	{
		AHollowHUD* HUD = AHollowHUD::Get(LocalPlayer(World));
		const UHollowPrimaryLayout* Layout = HUD ? HUD->GetPrimaryLayout() : nullptr;
		if (!Layout)
		{
			UE_LOG(LogHollowUI, Warning, TEXT("Hollow.UI.Menu: no Hollow UI on this machine."));
			return;
		}
		if (Args.Num() > 0 && Args[0].Equals(TEXT("Close"), ESearchCase::IgnoreCase))
		{
			const UCommonActivatableWidgetContainerBase* MenuLayer = Layout->GetLayer(HollowUITags::UI_Layer_Menu);
			if (UCommonActivatableWidget* Active = MenuLayer ? MenuLayer->GetActiveWidget() : nullptr)
			{
				Active->DeactivateWidget();
			}
			return;
		}
		HUD->OpenSystemMenu();
	}

	/** Usage: Hollow.UI.InputType <KBM|Gamepad> (what CommonUI would switch to on input from that device). */
	void InputType(const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* Player = LocalPlayer(World);
		UCommonInputSubsystem* Input = Player ? UCommonInputSubsystem::Get(Player->GetLocalPlayer()) : nullptr;
		if (!Input || Args.Num() < 1)
		{
			UE_LOG(LogHollowUI, Warning, TEXT("Hollow.UI.InputType <KBM|Gamepad> needs a local player."));
			return;
		}
		Input->SetCurrentInputType(Args[0].Equals(TEXT("Gamepad"), ESearchCase::IgnoreCase) ? ECommonInputType::Gamepad : ECommonInputType::MouseAndKeyboard);
	}

	/** How long Hollow.UI.PressKey holds a key unless told otherwise (buttons click on release). */
	constexpr float DefaultHoldSeconds = 0.1f;

	/**
	 * Presses and releases a key through Slate, like the keyboard or a gamepad would, so CommonUI routes it (actions,
	 * Back, focus navigation, device switching). Usage: Hollow.UI.PressKey <Key> [HoldSeconds]
	 */
	void PressKey(const TArray<FString>& Args, UWorld* World)
	{
		const FKey Key = Args.Num() > 0 ? FKey(*Args[0]) : FKey();
		if (!Key.IsValid() || !FSlateApplication::IsInitialized() || !World)
		{
			UE_LOG(LogHollowUI, Warning, TEXT("Hollow.UI.PressKey <Key> [HoldSeconds]: unknown key or no Slate."));
			return;
		}
		const float HoldSeconds = Args.Num() > 1 ? FCString::Atof(*Args[1]) : DefaultHoldSeconds;
		const FKeyEvent Event(Key, FModifierKeysState(), /*InUserIndex=*/0u, /*bInIsRepeat=*/false, /*InCharacterCode=*/0u, /*InKeyCode=*/0u);
		const bool bHandled = FSlateApplication::Get().ProcessKeyDownEvent(Event);
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.PressKey %s (handled %d)"), *Key.ToString(), bHandled);
		FTimerHandle Release;
		World->GetTimerManager().SetTimer(Release, FTimerDelegate::CreateLambda([Event]
		{
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().ProcessKeyUpEvent(Event);
			}
		}), FMath::Max(HoldSeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
	}

	/**
	 * A screenshot of the game view with the UI (the visor), also off screen and from Hollow.Dev.RunAfter / RunInRaid,
	 * which do not reach the viewport's own "shot". Usage: Hollow.UI.Shot [Name] (Saved/Screenshots/<Name>_NNNNN.png)
	 */
	void Shot(const TArray<FString>& Args)
	{
		const FString Name = Args.Num() > 0 ? Args[0] : FString(TEXT("Visor"));
		const FString File = FPaths::Combine(FPaths::ScreenShotDir(), Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, /*bInShowUI=*/true, /*bAddFilenameSuffix=*/true);
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Shot: %s"), *File);
	}

	/** LOC-01 (ADR-0048): prints or switches the language of the texts. Usage: Hollow.UI.Language [en|ru] [Keep] */
	void Language(const TArray<FString>& Args)
	{
		if (Args.Num() > 0)
		{
			const TOptional<EHollowLanguage> Wanted = HollowLanguage::Parse(Args[0]);
			if (!Wanted.IsSet())
			{
				UE_LOG(LogHollowUI, Warning, TEXT("Hollow.UI.Language: '%s' is not a language (en, ru)."), *Args[0]);
				return;
			}
			const bool bKeep = Args.Num() > 1 && Args[1].Equals(TEXT("Keep"), ESearchCase::IgnoreCase);
			if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
			{
				Settings->SetLanguage(Wanted.GetValue(), bKeep);
			}
			else
			{
				HollowLanguage::Apply(Wanted.GetValue());
			}
		}
		const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.UI.Language: %s (%s), kept '%s'"), LexToString(HollowLanguage::GetApplied()),
			HollowLanguage::CultureName(HollowLanguage::GetApplied()), Settings ? *Settings->GetSavedLanguage() : TEXT("-"));
	}

	/** NAR-01a (ADR-0049): fires a radio trigger on this machine. Usage: Hollow.Radio.Fire <Trigger> [Force] */
	void RadioFire(const TArray<FString>& Args, UWorld* World)
	{
		const UHollowVisorContext* Context = UHollowVisorContext::Get(LocalPlayer(World));
		UHollowRadioModel* Radio = Context ? Context->GetRadio() : nullptr;
		const FString Name = Args.Num() > 0 ? (Args[0].StartsWith(TEXT("Hollow.")) ? Args[0] : TEXT("Hollow.Radio.") + Args[0]) : FString();
		const FGameplayTag Trigger = FGameplayTag::RequestGameplayTag(FName(*Name), /*ErrorIfNotFound=*/false);
		if (!Radio || !Trigger.IsValid())
		{
			UE_LOG(LogHollowUI, Warning, TEXT("Hollow.Radio.Fire <Trigger> [Force]: no visor here, or '%s' is no Hollow.Radio.* trigger."), *Name);
			return;
		}
		const bool bForce = Args.Num() > 1 && Args[1].Equals(TEXT("Force"), ESearchCase::IgnoreCase);
		const bool bQueued = Radio->Fire(Trigger, bForce);
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.Radio.Fire %s%s: %s; %s"), *Trigger.ToString(), bForce ? TEXT(" (forced)") : TEXT(""),
			bQueued ? TEXT("queued") : TEXT("nothing to say"), *Radio->Describe());
	}

	/** NAR-01a: forgets every radio cue heard (the profile's and this raid's), so the briefing plays again. */
	void RadioReset(UWorld* World)
	{
		if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(World))
		{
			Profile->ForgetRadio();
		}
		const UHollowVisorContext* Context = UHollowVisorContext::Get(LocalPlayer(World));
		if (UHollowRadioModel* Radio = Context ? Context->GetRadio() : nullptr)
		{
			Radio->Forget();
		}
		UE_LOG(LogHollowUI, Display, TEXT("Hollow.Radio.Reset: every radio cue forgotten"));
	}

	FAutoConsoleCommandWithWorldAndArgs RadioFireCommand(
		TEXT("Hollow.Radio.Fire"),
		TEXT("Fires a radio trigger on this machine (Force ignores whether it was heard). Usage: Hollow.Radio.Fire <Core.Lifted|Hollow.Radio.*> [Force]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RadioFire));

	FAutoConsoleCommandWithWorld RadioResetCommand(
		TEXT("Hollow.Radio.Reset"),
		TEXT("Forgets every radio cue heard: the profile's (the briefing plays again) and this raid's."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RadioReset));

	FAutoConsoleCommand LanguageCommand(
		TEXT("Hollow.UI.Language"),
		TEXT("Prints or switches the language of the texts at once (Keep also saves it). Usage: Hollow.UI.Language [en|ru] [Keep]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Language));

	FAutoConsoleCommand ShotCommand(
		TEXT("Hollow.UI.Shot"),
		TEXT("Screenshot of the game view with the UI, off screen too. Usage: Hollow.UI.Shot [Name]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Shot));

	FAutoConsoleCommandWithWorldAndArgs PressKeyCommand(
		TEXT("Hollow.UI.PressKey"),
		TEXT("Presses and releases a key through Slate (keyboard or gamepad). Usage: Hollow.UI.PressKey <Key> [HoldSeconds]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PressKey));

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.UI.Dump"),
		TEXT("Prints this machine's UI: layers, input mode and device, visor bars, prompt, tool target, menu focus."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));

	FAutoConsoleCommandWithWorldAndArgs MenuCommand(
		TEXT("Hollow.UI.Menu"),
		TEXT("Opens or closes the system menu like Esc / Start and Back would. Usage: Hollow.UI.Menu <Open|Close>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Menu));

	FAutoConsoleCommandWithWorldAndArgs InputTypeCommand(
		TEXT("Hollow.UI.InputType"),
		TEXT("Switches the current input device type. Usage: Hollow.UI.InputType <KBM|Gamepad>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&InputType));
}

#endif // !UE_BUILD_SHIPPING
