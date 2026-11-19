#pragma once

#include "Attributes/HollowAttributeSetBase.h"

#include "HollowResonanceSet.generated.h"

/** Resonance of a player (Aether exposure). Replicated to everyone: drives squad-wide distortion and AI. */
UCLASS()
class HOLLOWGAS_API UHollowResonanceSet : public UHollowAttributeSetBase
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UHollowResonanceSet, Resonance);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowResonanceSet, MaxResonance);
	ATTRIBUTE_ACCESSORS_BASIC(UHollowResonanceSet, ResonanceGainMultiplier);

	virtual TConstArrayView<FHollowBoundedAttribute> GetBoundedAttributes() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION() void OnRep_Resonance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxResonance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ResonanceGainMultiplier(const FGameplayAttributeData& OldValue);

private:
	/** Current resonance. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Resonance, Category = "Hollow|Resonance", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Resonance;

	/** Resonance ceiling. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxResonance, Category = "Hollow|Resonance", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxResonance;

	/** Multiplier applied to resonance gain effects. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ResonanceGainMultiplier, Category = "Hollow|Resonance", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ResonanceGainMultiplier;
};
