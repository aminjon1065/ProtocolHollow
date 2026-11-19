#include "Attributes/HollowAttributeSetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAttributeSetBase)

void UHollowAttributeSetBase::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UHollowAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UHollowAttributeSetBase::PostAttributeChange(const FGameplayAttribute& Attribute, const float OldValue, const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	for (const FHollowBoundedAttribute& Bounded : GetBoundedAttributes())
	{
		if (Bounded.Max == Attribute && Bounded.Current.GetNumericValue(this) > NewValue)
		{
			if (UAbilitySystemComponent* AbilitySystem = GetOwningAbilitySystemComponent())
			{
				AbilitySystem->ApplyModToAttribute(Bounded.Current, EGameplayModOp::Override, NewValue);
			}
		}
	}
}

void UHollowAttributeSetBase::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (IsMetaAttribute(Attribute))
	{
		return;
	}

	for (const FHollowBoundedAttribute& Bounded : GetBoundedAttributes())
	{
		if (Bounded.Current == Attribute)
		{
			NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(0.f, Bounded.Max.GetNumericValue(this)));
			return;
		}
	}
	NewValue = FMath::Max(NewValue, 0.f);
}
