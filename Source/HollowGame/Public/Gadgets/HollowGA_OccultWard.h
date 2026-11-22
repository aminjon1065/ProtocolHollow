#pragma once

#include "Abilities/HollowGameplayAbility.h"
#include "Templates/SubclassOf.h"

#include "HollowGA_OccultWard.generated.h"

class AHollowOccultWard;

/**
 * Lays the Techno-Occultist's ritual ward at the feet (Gadget input, S6-03, ADR-0036), the alternative to the
 * Engineer's pulse mine. Granted by the ward item and costs one of it per use (ADR-0030). Predicted commit (the cooldown
 * effect, GE_Cooldown_OccultWard); any gadget's cooldown blocks it (Hollow.Cooldown.Gadget). The server spawns WardClass
 * on the floor below the pawn and takes back the player's previous ward: one burning ward per player.
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_OccultWard : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_OccultWard(const FObjectInitializer& ObjectInitializer);

	/** Server: the ward this ability laid last, while it burns. */
	AHollowOccultWard* GetActiveWard() const { return ActiveWard.Get(); }

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Gadget")
	TSubclassOf<AHollowOccultWard> WardClass;

private:
	void PlaceWard();

	TWeakObjectPtr<AHollowOccultWard> ActiveWard;
};
