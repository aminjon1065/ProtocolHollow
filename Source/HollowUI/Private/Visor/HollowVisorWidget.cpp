#include "Visor/HollowVisorWidget.h"

#include "Foundation/HollowHUD.h"
#include "Foundation/HollowUISettings.h"
#include "Input/CommonUIInputTypes.h"
#include "InputAction.h"
#include "Settings/HollowGameUserSettings.h"
#include "Visor/HollowWaypointLayerWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVisorWidget)

UHollowVisorWidget::UHollowVisorWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EHollowWidgetInputMode::Game;
}

void UHollowVisorWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (const UInputAction* Action = GetDefault<UHollowUISettings>()->SystemMenuAction.LoadSynchronous())
	{
		FBindUIActionArgs Args(Action, /*bShouldDisplayInActionBar=*/false, FSimpleDelegate::CreateUObject(this, &ThisClass::OpenSystemMenu));
		Args.InputMode = ECommonInputMode::Game;   // UI action bindings default to menu input; the visor runs in game input
		SystemMenuBinding = RegisterUIActionBinding(Args);
	}
	if (const UInputAction* Action = GetDefault<UHollowUISettings>()->ControlsHintAction.LoadSynchronous())
	{
		FBindUIActionArgs Args(Action, /*bShouldDisplayInActionBar=*/false, FSimpleDelegate::CreateUObject(this, &ThisClass::ToggleControlsHint));
		Args.InputMode = ECommonInputMode::Game;
		ControlsHintBinding = RegisterUIActionBinding(Args);
	}
}

void UHollowVisorWidget::NativeOnDeactivated()
{
	RemoveActionBinding(SystemMenuBinding);
	RemoveActionBinding(ControlsHintBinding);
	Super::NativeOnDeactivated();
}

void UHollowVisorWidget::OpenSystemMenu()
{
	if (AHollowHUD* HUD = AHollowHUD::Get(GetOwningPlayer()))
	{
		HUD->OpenSystemMenu();
	}
}

void UHollowVisorWidget::ToggleControlsHint()
{
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		Settings->SetControlsHintShown(!Settings->IsControlsHintShown());
	}
	if (Waypoints)
	{
		Waypoints->Reveal();
	}
}
