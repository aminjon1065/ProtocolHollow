#pragma once

#include "Attributes/HollowAttributeSetBase.h"

#include "HollowToolSet.generated.h"

/** Signature tool resources: fuel and heat. Private to the owner (COND_OwnerOnly). */
UCLASS()
class HOLLOWGAS_API UHollowToolSet : public UHollowAttributeSetBase
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UHollowToolSet, ToolFuel);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowToolSet, MaxToolFuel);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowToolSet, ToolHeat);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowToolSet, MaxToolHeat);

	virtual TConstArrayView<FHollowBoundedAttribute> GetBoundedAttributes() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION() void OnRep_ToolFuel(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxToolFuel(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ToolHeat(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxToolHeat(const FGameplayAttributeData& OldValue);

private:
	/** Current tool fuel. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ToolFuel, Category = "Hollow|Tool", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ToolFuel;

	/** Tool fuel capacity. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxToolFuel, Category = "Hollow|Tool", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxToolFuel;

	/** Current tool heat (overheats at max). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ToolHeat, Category = "Hollow|Tool", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ToolHeat;

	/** Tool heat limit. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxToolHeat, Category = "Hollow|Tool", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxToolHeat;
};
