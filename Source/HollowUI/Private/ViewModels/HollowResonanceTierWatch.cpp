#include "ViewModels/HollowResonanceTierWatch.h"

#include "AbilitySystemComponent.h"
#include "Resonance/HollowResonanceMath.h"

FHollowResonanceTierWatch::~FHollowResonanceTierWatch()
{
	Unbind();
}

void FHollowResonanceTierWatch::Bind(UAbilitySystemComponent* InAbilitySystem, FOnChanged InOnChanged)
{
	Unbind();
	AbilitySystem = InAbilitySystem;
	OnChanged = MoveTemp(InOnChanged);
	Tier = EHollowResonanceTier::Low;
	if (InAbilitySystem)
	{
		for (const EHollowResonanceTier Each : TEnumRange<EHollowResonanceTier>())
		{
			const FGameplayTag Tag = HollowResonanceMath::TierTag(Each);
			Handles.Emplace(Tag, InAbilitySystem->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddRaw(this, &FHollowResonanceTierWatch::HandleTag));
		}
		Tier = TierOf(*InAbilitySystem);
	}
	if (OnChanged)
	{
		OnChanged(Tier);
	}
}

void FHollowResonanceTierWatch::Unbind()
{
	if (UAbilitySystemComponent* Source = AbilitySystem.Get())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Handle : Handles)
		{
			Source->RegisterGameplayTagEvent(Handle.Key, EGameplayTagEventType::NewOrRemoved).Remove(Handle.Value);
		}
	}
	Handles.Reset();
	AbilitySystem.Reset();
}

EHollowResonanceTier FHollowResonanceTierWatch::TierOf(const UAbilitySystemComponent& InAbilitySystem)
{
	EHollowResonanceTier Highest = EHollowResonanceTier::Low;
	for (const EHollowResonanceTier Each : TEnumRange<EHollowResonanceTier>())
	{
		if (InAbilitySystem.HasMatchingGameplayTag(HollowResonanceMath::TierTag(Each)))
		{
			Highest = Each;
		}
	}
	return Highest;
}

void FHollowResonanceTierWatch::HandleTag(FGameplayTag Tag, int32 Count)
{
	const UAbilitySystemComponent* Source = AbilitySystem.Get();
	const EHollowResonanceTier NewTier = Source ? TierOf(*Source) : EHollowResonanceTier::Low;
	if (NewTier != Tier)
	{
		Tier = NewTier;
		if (OnChanged)
		{
			OnChanged(Tier);
		}
	}
}
