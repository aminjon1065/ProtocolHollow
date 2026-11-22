#pragma once

#include "Abilities/HollowGameplayAbility.h"
#include "Templates/SubclassOf.h"

#include "HollowGA_PulseMine.generated.h"

class AHollowPulseMine;

/**
 * Lays a kinetic pulse mine at the feet (Gadget input, ADR-0019). Granted by the pulse mine item and costs one of it
 * per use (ADR-0030): no mine in the raid inventory, no mine laid. Predicted commit (the cooldown effect,
 * GE_Cooldown_PulseMine); the server spawns MineClass on the floor below the pawn and takes back the player's
 * previous mine if it has not gone off: one active mine per player. Any gadget's cooldown blocks it (Hollow.Cooldown.Gadget,
 * S6-03): with mines and wards on the Gadget input, the one granted first is laid.
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_PulseMine : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_PulseMine(const FObjectInitializer& ObjectInitializer);

	/** Server: the mine this ability laid last, while it exists. */
	AHollowPulseMine* GetActiveMine() const { return ActiveMine.Get(); }

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Gadget")
	TSubclassOf<AHollowPulseMine> MineClass;

private:
	void PlaceMine();

	TWeakObjectPtr<AHollowPulseMine> ActiveMine;
};
