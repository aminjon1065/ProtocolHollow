// Development console commands for player classes (S6-01, ADR-0034): pick one, print every player's. Never shipped.
#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowToolSet.h"
#include "Classes/HollowClassComponent.h"
#include "Crafting/HollowCraftingRules.h"
#include "Engine/World.h"
#include "Framework/HollowPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Tags/HollowGameplayTags.h"
#include "Util/HollowEnumUtils.h"

#if !UE_BUILD_SHIPPING

namespace HollowClassConsole
{
	/** Usage: Hollow.Class.Select <Class>: the local profile plays it (the locker's transaction); in the hub the server follows. */
	void Select(const TArray<FString>& Args, UWorld* World)
	{
		UHollowProfileSubsystem* Profiles = UHollowProfileSubsystem::Get(World);
		if (!Profiles || Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Class.Select <Class> (needs a local profile)"));
			return;
		}
		const FHollowProfileTransaction Transaction = FHollowProfileTransaction::SelectClass(FPrimaryAssetId(TEXT("HollowClass"), FName(*Args[0])));
		const EHollowTransactionResult Result = Profiles->ApplyTransaction(Transaction);
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Class.Select: %s -> %s, plays %s with %d loadout slot(s)"), *Transaction.Describe(), *HollowEnumToString(Result),
			*Profiles->GetProfile().ClassId.ToString(), Profiles->GetProfile().Loadout.Num());
	}

	/** Every player's class on this machine: the replicated class, its tag on the ability system, the attributes it changes. */
	void Dump(UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (const UHollowProfileSubsystem* Profiles = UHollowProfileSubsystem::Get(World))
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Class.Dump: local profile plays %s (%d other class loadout(s))"), *Profiles->GetProfile().ClassId.ToString(),
				Profiles->GetProfile().ClassLoadouts.Num());
		}
		if (!GameState)
		{
			return;
		}
		for (const APlayerState* Each : GameState->PlayerArray)
		{
			const AHollowPlayerState* PlayerState = Cast<AHollowPlayerState>(Each);
			const UHollowClassComponent* Class = PlayerState ? PlayerState->GetClassComponent() : nullptr;
			const UHollowAbilitySystemComponent* AbilitySystem = PlayerState ? PlayerState->GetHollowAbilitySystemComponent() : nullptr;
			if (!Class || !AbilitySystem)
			{
				continue;
			}
			const TCHAR* Tag = AbilitySystem->HasMatchingGameplayTag(HollowTags::Class_Engineer) ? TEXT("Engineer")
				: AbilitySystem->HasMatchingGameplayTag(HollowTags::Class_Occultist) ? TEXT("Occultist") : TEXT("none");
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Class.Dump: %s plays %s, tag %s, max tool heat %.0f, armor %.0f, resonance gain x%.2f, %d abilities"),
				*PlayerState->GetPlayerName(), *Class->Describe(), Tag, AbilitySystem->GetNumericAttribute(UHollowToolSet::GetMaxToolHeatAttribute()),
				AbilitySystem->GetNumericAttribute(UHollowHealthSet::GetArmorAttribute()),
				AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetResonanceGainMultiplierAttribute()), AbilitySystem->GetActivatableAbilities().Num());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs SelectCommand(
		TEXT("Hollow.Class.Select"),
		TEXT("The local profile plays a class (as at the hub's locker). Usage: Hollow.Class.Select <Class>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Select));

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Class.Dump"),
		TEXT("Prints the local profile's class and every player's class, class tag and class attributes on this machine."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
