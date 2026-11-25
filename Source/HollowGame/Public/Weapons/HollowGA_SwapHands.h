#pragma once

#include "Abilities/HollowGameplayAbility.h"

#include "HollowGA_SwapHands.generated.h"

class UHollowHandsComponent;
enum class EHollowHandsMode : uint8;
enum class EHollowHandsRequest : uint8;

/**
 * Switches the hands between the tools and the weapon (GP-01, ADR-0044). The suit's set grants it three times, on the
 * Hands.Tools, Hands.Weapon and Hands.Cycle inputs: the spec's input says what it asks for. Predicted: the owner switches
 * at once and undoes it if the server refuses; the server switches its hands and the mode replicates to everyone else.
 * Its cooldown (UHollowHandsSwapEffect) is the moment the hands are busy: Hollow.State.Hands.Swapping. Activating it
 * cancels the running tools and weapon abilities. Blocked while carrying a core or dead.
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_SwapHands : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_SwapHands(const FObjectInitializer& ObjectInitializer);

	/** What a spec of this ability asks for, from its input tag (Cycle for any other). */
	static EHollowHandsRequest GetRequest(const FGameplayAbilitySpec& Spec);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** The mode this activation leads to, or the current one if it leads nowhere. */
	static EHollowHandsMode Target(const UHollowHandsComponent& Hands, const FGameplayAbilitySpec* Spec);
};
