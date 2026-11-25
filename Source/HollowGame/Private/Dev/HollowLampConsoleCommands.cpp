#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Interfaces/HollowLightSource.h"
#include "Tags/HollowGameplayTags.h"
#include "Util/HollowEnumUtils.h"

#if !UE_BUILD_SHIPPING

// Helmet lamp tools (S4-08, ADR-0026): switch the local player's lamp the way its key does, and every lamp on this machine.
namespace HollowLampConsolePrivate
{
	/** Activates GA_HelmetLamp of the local player (prediction, cooldown and the server's check as with the key). */
	void Toggle(UWorld* World)
	{
		const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		const AHollowExoFrameCharacter* Character = Player && Player->IsLocalController() ? Cast<AHollowExoFrameCharacter>(Player->GetPawn()) : nullptr;
		UHollowAbilitySystemComponent* AbilitySystem = Character ? Character->GetHollowAbilitySystemComponent() : nullptr;
		if (!AbilitySystem)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Lamp.Toggle needs a local player in an ExoFrame."));
			return;
		}
		const bool bActivated = AbilitySystem->TryActivateAbilitiesByTag(FGameplayTagContainer(HollowTags::Ability_HelmetLamp));
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Lamp.Toggle: %s (lamp now %s)"), bActivated ? TEXT("switched") : TEXT("refused"),
			Character->IsHelmetLampOn() ? TEXT("on") : TEXT("off"));
	}

	/** Every ExoFrame of this machine: whose, switched on, light shown, cone the AI avoids. */
	void Dump(UWorld* World)
	{
		for (TActorIterator<AHollowExoFrameCharacter> It(World); It; ++It)
		{
			const AHollowExoFrameCharacter* Character = *It;
			FHollowLightCone Cone;
			const bool bCone = Character->GetHelmetLampCone(Cone);
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Lamp.Dump: %s (%s%s): switched %s, light %d, cone %d, dead %d"), *Character->GetName(),
				*HollowEnumToString(Character->GetLocalRole()), Character->IsLocallyControlled() ? TEXT(", local") : TEXT(""),
				Character->IsHelmetLampOn() ? TEXT("on") : TEXT("off"), Character->GetHelmetLight()->IsVisible() ? 1 : 0, bCone ? 1 : 0, Character->IsDead() ? 1 : 0);
		}
	}

	FAutoConsoleCommandWithWorld ToggleCommand(
		TEXT("Hollow.Lamp.Toggle"),
		TEXT("Local player: switches the helmet lamp like its key (F / D-pad up): predicted, cooled down, checked by the server."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Toggle));

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Lamp.Dump"),
		TEXT("Every helmet lamp on this machine: switched, shown, the cone the Echo-Stalker avoids."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
