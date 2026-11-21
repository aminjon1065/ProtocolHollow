#include "Hub/HollowHubTerminalSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HollowGameLog.h"
#include "Hub/HollowHubTerminal.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHubTerminalSubsystem)

UHollowHubTerminalSubsystem* UHollowHubTerminalSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UHollowHubTerminalSubsystem>() : nullptr;
}

void UHollowHubTerminalSubsystem::Open(APlayerController* Player, AHollowHubTerminal* Terminal)
{
	if (!Player || !Terminal)
	{
		return;
	}
	if (GetOpenTerminal(Player) == Terminal)
	{
		return;
	}
	Close(Player);
	OpenTerminals.Add(Player, Terminal);
	UE_LOG(LogHollowGame, Log, TEXT("Hub: %s opens the %s terminal"), *Player->GetName(), *HollowEnumToString(Terminal->GetKind()));
	OnTerminalOpened.Broadcast(Player, Terminal);
}

void UHollowHubTerminalSubsystem::Close(APlayerController* Player)
{
	TWeakObjectPtr<AHollowHubTerminal> Terminal;
	if (Player && OpenTerminals.RemoveAndCopyValue(Player, Terminal) && Terminal.IsValid())
	{
		UE_LOG(LogHollowGame, Log, TEXT("Hub: %s closes the %s terminal"), *Player->GetName(), *HollowEnumToString(Terminal->GetKind()));
		OnTerminalClosed.Broadcast(Player, Terminal.Get());
	}
}

AHollowHubTerminal* UHollowHubTerminalSubsystem::GetOpenTerminal(const APlayerController* Player) const
{
	const TWeakObjectPtr<AHollowHubTerminal>* Terminal = OpenTerminals.Find(MakeWeakObjectPtr(const_cast<APlayerController*>(Player)));
	return Terminal ? Terminal->Get() : nullptr;
}
