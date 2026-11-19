#pragma once

#include "Attributes/HollowAttributeSetBase.h"

#include "HollowSurvivalSet.generated.h"

/** Survival attributes of a player: stamina, oxygen and body heat. Private to the owner (COND_OwnerOnly). */
UCLASS()
class HOLLOWGAS_API UHollowSurvivalSet : public UHollowAttributeSetBase
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UHollowSurvivalSet, Stamina);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowSurvivalSet, MaxStamina);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowSurvivalSet, Oxygen);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowSurvivalSet, MaxOxygen);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowSurvivalSet, Thermal);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowSurvivalSet, MaxThermal);

	virtual TConstArrayView<FHollowBoundedAttribute> GetBoundedAttributes() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Oxygen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxOxygen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Thermal(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxThermal(const FGameplayAttributeData& OldValue);

private:
	/** Current stamina. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Hollow|Survival", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Stamina;

	/** Stamina capacity. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Hollow|Survival", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxStamina;

	/** Current oxygen supply. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Oxygen, Category = "Hollow|Survival", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Oxygen;

	/** Oxygen capacity. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxOxygen, Category = "Hollow|Survival", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxOxygen;

	/** Current body heat reserve (cold drains it). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Thermal, Category = "Hollow|Survival", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Thermal;

	/** Body heat capacity. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxThermal, Category = "Hollow|Survival", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxThermal;
};
