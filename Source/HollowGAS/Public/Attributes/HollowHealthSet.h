#pragma once

#include "Attributes/HollowAttributeSetBase.h"

#include "HollowHealthSet.generated.h"

/**
 * Health of players and enemies. Replicated to everyone (teammates' visors, enemy feedback).
 * Damage and healing arrive as meta attributes (IncomingDamage / IncomingHealing, server only) that gameplay effects
 * write and this set converts into Health, clamped to [0, MaxHealth]. Damage math itself lives in executions (S3-04).
 */
UCLASS()
class HOLLOWGAS_API UHollowHealthSet : public UHollowAttributeSetBase
{
	GENERATED_BODY()

public:
	/** The causer of the effect (null for environment or development damage), the effect's instigator, the damage. */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnHollowOutOfHealth, AActor* /*EffectCauser*/, AActor* /*Instigator*/, float /*Damage*/);

	/** Server: health reached zero from above (once per life). Mutable: the set is read through const getters. */
	mutable FOnHollowOutOfHealth OnOutOfHealth;

	ATTRIBUTE_ACCESSORS_BASIC(UHollowHealthSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowHealthSet, MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowHealthSet, Armor);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowHealthSet, IncomingDamage);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowHealthSet, IncomingHealing);

	virtual TConstArrayView<FHollowBoundedAttribute> GetBoundedAttributes() const override;
	virtual bool IsMetaAttribute(const FGameplayAttribute& Attribute) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue);

private:
	/** Current health. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Hollow|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Health;

	/** Health capacity. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Hollow|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHealth;

	/** Flat damage reduction used by damage executions. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Hollow|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Armor;

	/** Meta attribute: damage to apply in the current execution. Server only, not replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "Hollow|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IncomingDamage;

	/** Meta attribute: healing to apply in the current execution. Server only, not replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "Hollow|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IncomingHealing;
};
