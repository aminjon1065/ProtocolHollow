#pragma once

#include "GameplayTagContainer.h"
#include "Resonance/HollowResonanceTypes.h"
#include "Templates/Function.h"

class UAbilitySystemComponent;

/**
 * Follows the squad Resonance tier a player carries as a Hollow.State.Resonance.* tag (S4-08) and calls back when it
 * changes (the visor's glass and readout, no Tick).
 */
class HOLLOWUI_API FHollowResonanceTierWatch : public FNoncopyable
{
public:
	using FOnChanged = TFunction<void(EHollowResonanceTier /*Tier*/)>;

	~FHollowResonanceTierWatch();

	/** Unbinds any previous source, binds this one and calls back once with its tier. */
	void Bind(UAbilitySystemComponent* InAbilitySystem, FOnChanged InOnChanged);
	void Unbind();

	bool IsBound() const { return AbilitySystem.IsValid(); }
	EHollowResonanceTier GetTier() const { return Tier; }

	/** The highest tier tag the ability system carries; Low without one. */
	static EHollowResonanceTier TierOf(const UAbilitySystemComponent& AbilitySystem);

private:
	void HandleTag(FGameplayTag Tag, int32 Count);

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TArray<TPair<FGameplayTag, FDelegateHandle>, TInlineAllocator<4>> Handles;
	FOnChanged OnChanged;
	EHollowResonanceTier Tier = EHollowResonanceTier::Low;
};
