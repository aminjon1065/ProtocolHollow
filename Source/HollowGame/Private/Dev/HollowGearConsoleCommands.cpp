// Development console commands for the players' gear (S6-04, ADR-0037): what the occult relics change. Never shipped.
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/HollowGearSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Tags/HollowGameplayTags.h"

#if !UE_BUILD_SHIPPING

namespace HollowGearConsole
{
	/**
	 * Any machine: every player's gear multipliers and Resonance gain (the gear is replicated to its owner only: another
	 * player's shows neutral on a client) and, for the local pawn, how fast it may move now.
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
			const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
			const ACharacter* Character = Cast<ACharacter>(Player ? Player->GetPawn() : nullptr);
			const bool bLocal = Character && Character->IsLocallyControlled();
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Gear.Dump: player %s (%s) footsteps x%.2f, tool work x%.2f, carry speed x%.2f, resonance gain x%.2f%s"),
				*Player->GetPlayerName(), *GetNameSafe(Character),
				UHollowGearSet::GetMultiplier(AbilitySystem, UHollowGearSet::GetFootstepNoiseMultiplierAttribute()),
				UHollowGearSet::GetMultiplier(AbilitySystem, UHollowGearSet::GetToolWorkMultiplierAttribute()),
				UHollowGearSet::GetMultiplier(AbilitySystem, UHollowGearSet::GetCarrySpeedMultiplierAttribute()),
				AbilitySystem ? AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetResonanceGainMultiplierAttribute()) : 0.f,
				bLocal ? *FString::Printf(TEXT(", local, max speed now %.0f"), Character->GetCharacterMovement()->GetMaxSpeed()) : TEXT(""));
		}
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Gear.Dump"),
		TEXT("Prints every player's gear multipliers (the occult relics) and Resonance gain; the local pawn's max speed now."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
