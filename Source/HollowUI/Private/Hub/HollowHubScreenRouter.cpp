#include "Hub/HollowHubScreenRouter.h"

#include "Foundation/HollowPrimaryLayout.h"
#include "Foundation/HollowUISettings.h"
#include "GameFramework/PlayerController.h"
#include "HollowUILog.h"
#include "HollowUITags.h"
#include "Hub/HollowHubScreenWidget.h"
#include "Hub/HollowHubTerminal.h"
#include "Hub/HollowHubTerminalSubsystem.h"
#include "Util/HollowEnumUtils.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHubScreenRouter)

void UHollowHubScreenRouter::Initialize(UHollowPrimaryLayout* InLayout, APlayerController* InPlayer)
{
	Layout = InLayout;
	Player = InPlayer;
	UHollowHubTerminalSubsystem* Subsystem = InPlayer ? UHollowHubTerminalSubsystem::Get(InPlayer->GetWorld()) : nullptr;
	Terminals = Subsystem;
	if (Subsystem)
	{
		OpenedHandle = Subsystem->OnTerminalOpened.AddUObject(this, &ThisClass::HandleOpened);
		ClosedHandle = Subsystem->OnTerminalClosed.AddUObject(this, &ThisClass::HandleClosed);
	}
}

void UHollowHubScreenRouter::Deinitialize()
{
	if (UHollowHubTerminalSubsystem* Subsystem = Terminals.Get())
	{
		Subsystem->OnTerminalOpened.Remove(OpenedHandle);
		Subsystem->OnTerminalClosed.Remove(ClosedHandle);
	}
	Terminals.Reset();
	Screen = nullptr;
}

UHollowHubScreenWidget* UHollowHubScreenRouter::GetScreen() const
{
	return Screen && Screen->IsActivated() ? Screen.Get() : nullptr;
}

TSubclassOf<UHollowHubScreenWidget> UHollowHubScreenRouter::ScreenClass(const EHollowTerminalKind Kind)
{
	const UHollowUISettings* Settings = GetDefault<UHollowUISettings>();
	switch (Kind)
	{
	case EHollowTerminalKind::Contracts:
		return Settings->ContractScreenClass.LoadSynchronous();
	case EHollowTerminalKind::Workbench:
		return Settings->WorkbenchScreenClass.LoadSynchronous();
	case EHollowTerminalKind::Locker:
		return Settings->LoadoutScreenClass.LoadSynchronous();
	}
	return nullptr;
}

void UHollowHubScreenRouter::HandleOpened(APlayerController* InPlayer, AHollowHubTerminal* Terminal)
{
	UHollowPrimaryLayout* TheLayout = Layout.Get();
	UCommonActivatableWidgetContainerBase* MenuLayer = TheLayout ? TheLayout->GetLayer(HollowUITags::UI_Layer_Menu) : nullptr;
	if (InPlayer != Player.Get() || !Terminal || !MenuLayer)
	{
		return;
	}
	const TSubclassOf<UHollowHubScreenWidget> Class = ScreenClass(Terminal->GetKind());
	if (!ensureMsgf(Class, TEXT("No hub screen for the %s terminal (Hollow UI settings)"), *HollowEnumToString(Terminal->GetKind())))
	{
		return;
	}
	if (UHollowHubScreenWidget* Previous = GetScreen())
	{
		Previous->DeactivateWidget();
	}
	// Who and where, before it is shown: it builds its rows on activation.
	Screen = MenuLayer->AddWidget<UHollowHubScreenWidget>(Class, [InPlayer, Terminal](UHollowHubScreenWidget& NewScreen)
	{
		NewScreen.OpenFor(InPlayer, Terminal);
	});
	UE_LOG(LogHollowUI, Log, TEXT("Hub: %s shows %s for the %s terminal"), *InPlayer->GetName(), *GetNameSafe(Screen), *HollowEnumToString(Terminal->GetKind()));
}

void UHollowHubScreenRouter::HandleClosed(APlayerController* InPlayer, AHollowHubTerminal* Terminal)
{
	UHollowHubScreenWidget* Shown = GetScreen();
	if (InPlayer == Player.Get() && Shown && Shown->GetTerminal() == Terminal)
	{
		Shown->DeactivateWidget();
	}
}
