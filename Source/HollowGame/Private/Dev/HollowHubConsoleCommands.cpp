// Development console commands of the hub shuttle (ADR-0032): terminals and contracts, until the hub's screens (S5-04).
// Never shipped.
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/HollowPlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Hub/HollowContractDefinition.h"
#include "Hub/HollowHubStateComponent.h"
#include "Hub/HollowHubTerminal.h"
#include "Hub/HollowHubTerminalSubsystem.h"
#include "Raid/HollowRaidLauncher.h"
#include "Util/HollowEnumUtils.h"

#if !UE_BUILD_SHIPPING

namespace HollowHubConsole
{
	/** Where a placed player looks: the middle of a terminal's screen over its floor (create_hub_assets.py). */
	constexpr double TerminalScreenHeight = 105.0;

	AHollowPlayerController* LocalPlayer(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? Cast<AHollowPlayerController>(GameInstance->GetFirstLocalPlayerController(World)) : nullptr;
	}

	AHollowHubTerminal* FindTerminal(UWorld* World, const FString& KindName)
	{
		for (TActorIterator<AHollowHubTerminal> It(World); It; ++It)
		{
			if (HollowEnumToString(It->GetKind()).Equals(KindName, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** The contract, the terminals of this world and which of them the local player has open. */
	void Dump(UWorld* World)
	{
		const UHollowHubStateComponent* Hub = UHollowHubStateComponent::Get(World);
		const UHollowHubTerminalSubsystem* Terminals = UHollowHubTerminalSubsystem::Get(World);
		const AHollowHubTerminal* Open = Terminals ? Terminals->GetOpenTerminal(LocalPlayer(World)) : nullptr;
		const UHollowContractDefinition* Contract = Hub ? UHollowContractDefinition::Load(Hub->GetSelectedContract()) : nullptr;
		FString Rewards;
		if (Contract)
		{
			for (const FPrimaryAssetId& Item : Contract->RewardItems)
			{
				Rewards += (Rewards.IsEmpty() ? TEXT("") : TEXT(",")) + Item.PrimaryAssetName.ToString();
			}
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Hub.Dump: contract %s (sector %s \"%s\", reward %s) | open terminal %s | can start a raid %d"),
			Hub && Hub->GetSelectedContract().IsValid() ? *Hub->GetSelectedContract().ToString() : TEXT("none"),
			Contract ? *Contract->SectorFeature : TEXT("none"), Contract ? *Contract->SectorName.ToString() : TEXT(""), Rewards.IsEmpty() ? TEXT("none") : *Rewards,
			Open ? *HollowEnumToString(Open->GetKind()) : TEXT("none"), HollowRaidLauncher::CanStartRaid(World));
		for (TActorIterator<AHollowHubTerminal> It(World); It; ++It)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Hub.Dump: terminal %s %s at %s"), *HollowEnumToString(It->GetKind()), *It->GetName(),
				*It->GetActorLocation().ToCompactString());
		}
	}

	/** Usage: Hollow.Hub.Select <Contract> (the host's choice; a client is refused by the server) */
	void Select(const TArray<FString>& Args, UWorld* World)
	{
		AHollowPlayerController* Player = LocalPlayer(World);
		if (Args.IsEmpty() || !Player)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Hub.Select <Contract>"));
			return;
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Hub.Select: asks for %s"), *Args[0]);
		Player->Server_SelectContract(FPrimaryAssetId(UHollowContractDefinition::AssetType, FName(*Args[0])));
	}

	/** The host sends the squad on the selected contract (the system menu's Start raid takes the same path). */
	void Launch(UWorld* World)
	{
		if (!HollowRaidLauncher::CanStartRaid(World))
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Hub.Launch: only the host, in the hub."));
			return;
		}
		HollowRaidLauncher::Travel(World);
	}

	void Close(UWorld* World)
	{
		if (UHollowHubTerminalSubsystem* Terminals = UHollowHubTerminalSubsystem::Get(World))
		{
			Terminals->Close(LocalPlayer(World));
		}
	}

	/**
	 * Usage: Hollow.Hub.Open <Contracts|Workbench|Locker>: opens that terminal for the local player on this machine, as
	 * the server's answer to E would (its screen shows, S5-04), skipping the walk and the server's checks.
	 */
	void Open(const TArray<FString>& Args, UWorld* World)
	{
		UHollowHubTerminalSubsystem* Terminals = UHollowHubTerminalSubsystem::Get(World);
		AHollowHubTerminal* Terminal = Args.Num() > 0 ? FindTerminal(World, Args[0]) : nullptr;
		AHollowPlayerController* Player = LocalPlayer(World);
		if (!Terminals || !Terminal || !Player)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Hub.Open <Contracts|Workbench|Locker>: no such terminal or no local player here."));
			return;
		}
		Terminals->Open(Player, Terminal);
	}

	/** Usage: Hollow.Dev.GoToTerminal <Contracts|Workbench|Locker> [All|Others] (host): in front of it, facing it. */
	void GoToTerminal(const TArray<FString>& Args, UWorld* World)
	{
		AHollowHubTerminal* Terminal = Args.IsEmpty() || !World ? nullptr : FindTerminal(World, Args[0]);
		if (!Terminal || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (host, in the hub): Hollow.Dev.GoToTerminal <Contracts|Workbench|Locker> [All|Others]"));
			return;
		}
		const bool bAll = Args.Num() > 1 && Args[1].Equals(TEXT("All"), ESearchCase::IgnoreCase);
		const bool bOthers = Args.Num() > 1 && Args[1].Equals(TEXT("Others"), ESearchCase::IgnoreCase);
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Player = It->Get();
			APawn* Pawn = Player ? Player->GetPawn() : nullptr;
			if (!Pawn || !(bAll || Player->IsLocalController() != bOthers))
			{
				continue;
			}
			const FVector Use = Terminal->GetUseLocation();
			const FVector Location(Use.X, Use.Y, Pawn->GetActorLocation().Z);
			Pawn->TeleportTo(Location, (Terminal->GetActorLocation() - Location).GetSafeNormal2D().Rotation());
			// The eyes on the terminal's middle (its screen), the way a player looks at it.
			const FRotator Look = (Terminal->GetActorLocation() + FVector(0.0, 0.0, TerminalScreenHeight) - Pawn->GetPawnViewLocation()).Rotation();
			Player->SetControlRotation(Look);
			Player->ClientSetRotation(Look);
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.GoToTerminal: %s at the %s terminal"), *Player->GetName(), *HollowEnumToString(Terminal->GetKind()));
		}
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Hub.Dump"),
		TEXT("Prints the hub: the selected contract, the terminals and the one the local player has open."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));

	FAutoConsoleCommandWithWorldAndArgs SelectCommand(
		TEXT("Hollow.Hub.Select"),
		TEXT("Asks the host's server to select a contract (only the host may). Usage: Hollow.Hub.Select <Contract>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Select));

	FAutoConsoleCommandWithWorld LaunchCommand(
		TEXT("Hollow.Hub.Launch"),
		TEXT("Host, in the hub: sends the squad into a raid on the selected contract."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Launch));

	FAutoConsoleCommandWithWorldAndArgs OpenCommand(
		TEXT("Hollow.Hub.Open"),
		TEXT("Opens a hub terminal (its screen) for the local player without walking there. Usage: Hollow.Hub.Open <Contracts|Workbench|Locker>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Open));

	FAutoConsoleCommandWithWorld CloseCommand(
		TEXT("Hollow.Hub.Close"),
		TEXT("Closes the local player's open hub terminal."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Close));

	FAutoConsoleCommandWithWorldAndArgs GoToTerminalCommand(
		TEXT("Hollow.Dev.GoToTerminal"),
		TEXT("Host: puts the local player (All: everyone, Others: everyone else) in front of a hub terminal, facing it. Usage: Hollow.Dev.GoToTerminal <Kind> [All|Others]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GoToTerminal));
}

#endif // !UE_BUILD_SHIPPING
