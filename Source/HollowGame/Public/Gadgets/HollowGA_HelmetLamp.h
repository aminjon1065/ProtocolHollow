#pragma once

#include "Abilities/HollowGameplayAbility.h"
#include "GameplayPrediction.h"

#include "HollowGA_HelmetLamp.generated.h"

class AHollowExoFrameCharacter;

/**
 * Switches the helmet lamp (HelmetLamp input, S4-08, ADR-0026). Local prediction: the owner's lamp switches at once
 * and the server switches its own copy, which replicates to everyone else. Switches toggle, so the owner and the
 * server agree after any sequence: a switch the server rejects (dead, cooling down) is switched back on the owner.
 * The cooldown effect (GE_Cooldown_HelmetLamp) keeps the lamp from strobing.
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_HelmetLamp : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_HelmetLamp(const FObjectInitializer& ObjectInitializer);

	/** Owner: switches the lamp and switches it back when OnRejected fires (the server rejected the activation's prediction key). */
	static void PredictSwitch(AHollowExoFrameCharacter& Character, FPredictionKeyEvent& OnRejected);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
