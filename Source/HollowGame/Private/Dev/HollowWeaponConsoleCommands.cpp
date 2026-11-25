// Development console commands for the weapons (GP-01, ADR-0044): what each player holds and how its hits fared. Never shipped.
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/HollowWeaponSet.h"
#include "Data/HollowWeaponDefinition.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Weapons/HollowHandsComponent.h"

#if !UE_BUILD_SHIPPING

namespace HollowWeaponConsole
{
	/**
	 * Any machine: every player's hands, weapon, magazine and rounds carried (the magazine and the inventory replicate to
	 * their owner only: another player's show 0 on a client) and, on the server, its hits taken and refused.
	 */
	void Dump(UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState)
		{
			return;
		}
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			const UHollowHandsComponent* Hands = UHollowHandsComponent::Get(Player ? Player->GetPawn() : nullptr);
			if (!Hands)
			{
				continue;
			}
			const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
			const UHollowInventoryComponent* Inventory = UHollowInventoryComponent::Get(Player);
			const UHollowWeaponDefinition* Weapon = Hands->GetWeapon();
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Weapon.Dump: player %s: %s; magazine %.0f/%.0f, carried %d"), *Player->GetPlayerName(), *Hands->Describe(),
				AbilitySystem ? AbilitySystem->GetNumericAttribute(UHollowWeaponSet::GetMagazineAttribute()) : 0.f,
				AbilitySystem ? AbilitySystem->GetNumericAttribute(UHollowWeaponSet::GetMaxMagazineAttribute()) : 0.f,
				Inventory && Weapon ? Inventory->GetCount(Weapon->AmmoItem) : 0);
		}
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Weapon.Dump"),
		TEXT("Prints every player's hands, weapon, magazine, rounds carried and (server) the owner's hits taken and refused."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
