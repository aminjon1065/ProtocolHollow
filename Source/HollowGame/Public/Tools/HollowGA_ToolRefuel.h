#pragma once

#include "Abilities/HollowGameplayAbility.h"
#include "Templates/SubclassOf.h"

#include "HollowGA_ToolRefuel.generated.h"

class UGameplayEffect;

/**
 * Burns a fuel cell into the tool tank (Reload input, S5-01, ADR-0030). Granted by the fuel cell item and costs one of
 * it per use, so a player refuels only with the cells they brought; refused while the tank is full (no cell wasted).
 * Predicted: the commit (the cooldown, GE_Cooldown_ToolRefuel) and RefuelEffect on the owner; the server takes the cell.
 * Its tag sits under Hollow.Ability.Tool: carrying a core or being dead blocks it like the tools.
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_ToolRefuel : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_ToolRefuel(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Instant effect adding one cell's fuel to ToolFuel (the tool set clamps it to MaxToolFuel). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Tool")
	TSubclassOf<UGameplayEffect> RefuelEffect;
};
