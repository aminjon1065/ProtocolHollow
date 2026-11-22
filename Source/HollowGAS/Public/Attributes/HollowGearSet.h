#pragma once

#include "Attributes/HollowAttributeSetBase.h"

#include "HollowGearSet.generated.h"

/**
 * What a player's gear changes (S6-04, ADR-0037): multipliers the occult relics' effects modify (GE_Relic_*), neutral
 * (1) without them. Each is read where its work is done: the footstep noise (its loudness for the AI), a continuous
 * tool's work per sample (cutter, welder), the speed of a heavy carry (the movement component, predicted: the owner
 * needs it too). Private to the owner (COND_OwnerOnly).
 */
UCLASS()
class HOLLOWGAS_API UHollowGearSet : public UHollowAttributeSetBase
{
	GENERATED_BODY()

public:
	UHollowGearSet();

	ATTRIBUTE_ACCESSORS_BASIC(UHollowGearSet, FootstepNoiseMultiplier);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowGearSet, ToolWorkMultiplier);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowGearSet, CarrySpeedMultiplier);

	/** Value of Attribute on AbilitySystem, or neutral (1) without this set (an enemy, a bare test actor). */
	static float GetMultiplier(const UAbilitySystemComponent* AbilitySystem, const FGameplayAttribute& Attribute);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION() void OnRep_FootstepNoiseMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ToolWorkMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CarrySpeedMultiplier(const FGameplayAttributeData& OldValue);

private:
	/** Loudness of the player's footsteps for the AI (Ring of Silence). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FootstepNoiseMultiplier, Category = "Hollow|Gear", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData FootstepNoiseMultiplier;

	/** Work a cutter or welder does per sample (Plasma Capacitor). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ToolWorkMultiplier, Category = "Hollow|Gear", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ToolWorkMultiplier;

	/** Speed while carrying something heavy (Kinetic Brace). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CarrySpeedMultiplier, Category = "Hollow|Gear", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData CarrySpeedMultiplier;
};
