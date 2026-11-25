#pragma once

#include "Attributes/HollowAttributeSetBase.h"

#include "HollowWeaponSet.generated.h"

/**
 * The magazine of the weapon in the loadout (GP-01, ADR-0044): rounds in it and its size. Private to the owner
 * (COND_OwnerOnly); a shot's cost and a reload change it through gameplay effects, predicted on the owner like the
 * tools' fuel. The rounds behind it are items of the raid inventory.
 */
UCLASS()
class HOLLOWGAS_API UHollowWeaponSet : public UHollowAttributeSetBase
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UHollowWeaponSet, Magazine);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowWeaponSet, MaxMagazine);

	virtual TConstArrayView<FHollowBoundedAttribute> GetBoundedAttributes() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION() void OnRep_Magazine(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxMagazine(const FGameplayAttributeData& OldValue);

private:
	/** Rounds in the magazine. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Magazine, Category = "Hollow|Weapon", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Magazine;

	/** Rounds the magazine holds (the weapon's MagazineSize, set by its item). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMagazine, Category = "Hollow|Weapon", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxMagazine;
};
