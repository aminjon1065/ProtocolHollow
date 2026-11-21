// Development console commands of the raid inventory (ADR-0030). Never shipped.
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Inventory/HollowLoadoutRules.h"
#include "Items/HollowItemDefinition.h"

#if !UE_BUILD_SHIPPING

namespace HollowInventoryConsole
{
	/** Whether this machine owns State (its own player: the only inventory it receives on a client). */
	bool IsLocal(const APlayerState* State)
	{
		const APlayerController* Controller = State ? State->GetPlayerController() : nullptr;
		return Controller && Controller->IsLocalController();
	}

	/** Every player's inventory on the host; on a client its own (the others' are not sent to it). */
	void Dump(UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Inventory.Dump: no game state."));
			return;
		}
		const bool bHost = World->GetNetMode() != NM_Client;
		for (const APlayerState* State : GameState->PlayerArray)
		{
			const UHollowInventoryComponent* Inventory = UHollowInventoryComponent::Get(State);
			if (!Inventory || (!bHost && !IsLocal(State)))
			{
				continue;
			}
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Inventory.Dump: %s%s: %s | spent %s"), *State->GetPlayerName(), IsLocal(State) ? TEXT(" (local)") : TEXT(""),
				*Inventory->Describe(), *HollowLoadoutRules::Describe(Inventory->GetSpent()));
		}
	}

	/** Usage: Hollow.Inventory.Give <Item> [Count] [All] (host) */
	void Give(const TArray<FString>& Args, UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState || World->GetNetMode() == NM_Client || Args.IsEmpty() || !UAssetManager::IsInitialized())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (host): Hollow.Inventory.Give <Item> [Count] [All]"));
			return;
		}
		const FPrimaryAssetId Id(TEXT("HollowItem"), FName(*Args[0]));
		UHollowItemDefinition* Item = Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad());
		const int32 Count = Args.Num() > 1 && Args[1].IsNumeric() ? FCString::Atoi(*Args[1]) : 1;
		const bool bAll = Args.ContainsByPredicate([](const FString& Arg) { return Arg.Equals(TEXT("All"), ESearchCase::IgnoreCase); });
		if (!Item)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Inventory.Give: unknown item %s"), *Id.ToString());
			return;
		}
		for (const APlayerState* State : GameState->PlayerArray)
		{
			UHollowInventoryComponent* Inventory = UHollowInventoryComponent::Get(State);
			if (Inventory && (bAll || IsLocal(State)))
			{
				Inventory->AddItem(Item, Count);
				UE_LOG(LogHollowGame, Display, TEXT("Hollow.Inventory.Give: %s gets %d %s: %s"), *State->GetPlayerName(), Count, *Item->GetName(), *Inventory->Describe());
			}
		}
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Inventory.Dump"),
		TEXT("Prints the raid inventories: every player's on the host, the own one on a client (held/taken, spent)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));

	FAutoConsoleCommandWithWorldAndArgs GiveCommand(
		TEXT("Hollow.Inventory.Give"),
		TEXT("Host: adds an item to the local player's raid inventory (All: every player's), granting its abilities. Usage: Hollow.Inventory.Give <Item> [Count] [All]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Give));
}

#endif // !UE_BUILD_SHIPPING
