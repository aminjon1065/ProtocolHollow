#include "Foundation/HollowHUD.h"

#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Foundation/HollowPrimaryLayout.h"
#include "Foundation/HollowUISettings.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Raid/HollowDebriefWidget.h"
#include "GameFramework/PlayerController.h"
#include "HollowUILog.h"
#include "HollowUITags.h"
#include "Hub/HollowHubScreenRouter.h"
#include "InputMappingContext.h"
#include "Menus/HollowMenuRules.h"
#include "ViewModels/HollowVisorContext.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHUD)

AHollowHUD::AHollowHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;   // the visor is event driven (CLAUDE.md sec. 1)
}

AHollowHUD* AHollowHUD::Get(const APlayerController* Player)
{
	return Player ? Cast<AHollowHUD>(Player->GetHUD()) : nullptr;
}

void AHollowHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* Player = GetOwningPlayerController();
	if (!Player || !Player->IsLocalController())
	{
		return;
	}
	const UHollowUISettings* Settings = GetDefault<UHollowUISettings>();
	VisorContext = NewObject<UHollowVisorContext>(this);
	VisorContext->Initialize(Player);

	// Small classes, loaded once per map behind the loading screen.
	UIMappingContext = Settings->UIMappingContext.LoadSynchronous();
	UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player->GetLocalPlayer());
	if (Input && UIMappingContext)
	{
		Input->AddMappingContext(UIMappingContext, Settings->UIMappingPriority);
	}
	const TSubclassOf<UHollowPrimaryLayout> LayoutClass = Settings->PrimaryLayoutClass.LoadSynchronous();
	if (!ensureMsgf(LayoutClass, TEXT("HollowUISettings.PrimaryLayoutClass is not set")))
	{
		return;
	}
	Layout = CreateWidget<UHollowPrimaryLayout>(Player, LayoutClass);
	Layout->AddToPlayerScreen();
	// The front end has no body to show a visor for: its main menu instead (a debrief below still shows on top of it).
	const bool bFrontEnd = HollowMenuRules::IsFrontEnd(GetWorld());
	Layout->PushWidgetToLayer(bFrontEnd ? HollowUITags::UI_Layer_Menu : HollowUITags::UI_Layer_Game,
		(bFrontEnd ? Settings->MainMenuClass : Settings->VisorClass).LoadSynchronous());
	UE_LOG(LogHollowUI, Log, TEXT("%s: UI up for %s (%s)"), *GetName(), *Player->GetName(), bFrontEnd ? TEXT("main menu") : TEXT("visor"));
	if (!bFrontEnd)
	{
		// The hub's terminals open their screens here (S5-04); a map without terminals never asks.
		HubScreens = NewObject<UHollowHubScreenRouter>(this);
		HubScreens->Initialize(Layout, Player);
	}

	// A raid resolved now, or one the player has not seen yet (after travel, a lost host, a crash).
	if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this))
	{
		RaidReportHandle = Profile->OnRaidReport.AddUObject(this, &ThisClass::HandleRaidReport);
		if (const FHollowRaidReport* Pending = Profile->GetPendingReport())
		{
			ShowDebrief(*Pending);
		}
	}
}

void AHollowHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this))
	{
		Profile->OnRaidReport.Remove(RaidReportHandle);
	}
	if (HubScreens)
	{
		HubScreens->Deinitialize();
		HubScreens = nullptr;
	}
	if (Layout)
	{
		Layout->RemoveFromParent();
		Layout = nullptr;
	}
	const APlayerController* Player = GetOwningPlayerController();
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player->GetLocalPlayer()) : nullptr;
	if (Input && UIMappingContext)
	{
		Input->RemoveMappingContext(UIMappingContext);
	}
	if (VisorContext)
	{
		VisorContext->Deinitialize();
	}
	Super::EndPlay(EndPlayReason);
}

UCommonActivatableWidget* AHollowHUD::OpenSystemMenu()
{
	const UCommonActivatableWidgetContainerBase* MenuLayer = Layout ? Layout->GetLayer(HollowUITags::UI_Layer_Menu) : nullptr;
	if (!MenuLayer)
	{
		return nullptr;
	}
	if (UCommonActivatableWidget* Active = MenuLayer->GetActiveWidget())
	{
		return Active;
	}
	return Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Menu, GetDefault<UHollowUISettings>()->SystemMenuClass.LoadSynchronous());
}

void AHollowHUD::HandleRaidReport(const FHollowRaidReport& Report)
{
	ShowDebrief(Report);
}

UHollowDebriefWidget* AHollowHUD::ShowDebrief(const FHollowRaidReport& Report)
{
	if (!Layout)
	{
		return nullptr;
	}
	if (!Debrief || !Debrief->IsActivated())
	{
		Debrief = Cast<UHollowDebriefWidget>(Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Menu, GetDefault<UHollowUISettings>()->DebriefClass.LoadSynchronous()));
	}
	if (Debrief)
	{
		Debrief->SetReport(Report);
		UE_LOG(LogHollowUI, Log, TEXT("%s: debrief %s"), *GetName(), *Debrief->GetTitle().ToString());
	}
	return Debrief;
}
