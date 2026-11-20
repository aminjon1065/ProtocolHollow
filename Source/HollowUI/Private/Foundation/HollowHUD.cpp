#include "Foundation/HollowHUD.h"

#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Foundation/HollowPrimaryLayout.h"
#include "Foundation/HollowUISettings.h"
#include "GameFramework/PlayerController.h"
#include "HollowUILog.h"
#include "HollowUITags.h"
#include "InputMappingContext.h"
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
	Layout->PushWidgetToLayer(HollowUITags::UI_Layer_Game, Settings->VisorClass.LoadSynchronous());
	UE_LOG(LogHollowUI, Log, TEXT("%s: UI up for %s"), *GetName(), *Player->GetName());
}

void AHollowHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
