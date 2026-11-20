#include "ViewModels/HollowAttributeWatch.h"

#include "AbilitySystemComponent.h"

FHollowAttributeWatch::~FHollowAttributeWatch()
{
	Unbind();
}

void FHollowAttributeWatch::Bind(UAbilitySystemComponent* InAbilitySystem, const FGameplayAttribute& InAttribute, const FGameplayAttribute& InMaxAttribute,
	FOnChanged InOnChanged)
{
	Unbind();
	if (!InAbilitySystem || !InAttribute.IsValid())
	{
		return;
	}
	AbilitySystem = InAbilitySystem;
	Attribute = InAttribute;
	MaxAttribute = InMaxAttribute;
	OnChanged = MoveTemp(InOnChanged);
	ValueHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(Attribute).AddRaw(this, &FHollowAttributeWatch::HandleChange);
	if (MaxAttribute.IsValid())
	{
		MaxHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(MaxAttribute).AddRaw(this, &FHollowAttributeWatch::HandleChange);
	}
	Refresh();
}

void FHollowAttributeWatch::Unbind()
{
	if (UAbilitySystemComponent* Source = AbilitySystem.Get())
	{
		Source->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(ValueHandle);
		if (MaxAttribute.IsValid())
		{
			Source->GetGameplayAttributeValueChangeDelegate(MaxAttribute).Remove(MaxHandle);
		}
	}
	AbilitySystem.Reset();
	OnChanged = nullptr;
	Value = 0.f;
	Max = 0.f;
}

float FHollowAttributeWatch::Ratio(const float InValue, const float InMax)
{
	return InMax > 0.f ? FMath::Clamp(InValue / InMax, 0.f, 1.f) : 0.f;
}

void FHollowAttributeWatch::HandleChange(const FOnAttributeChangeData& Change)
{
	Refresh();
}

void FHollowAttributeWatch::Refresh()
{
	const UAbilitySystemComponent* Source = AbilitySystem.Get();
	if (!Source)
	{
		return;
	}
	bool bFound = false;
	Value = Source->GetGameplayAttributeValue(Attribute, bFound);
	Max = MaxAttribute.IsValid() ? Source->GetGameplayAttributeValue(MaxAttribute, bFound) : 0.f;
	if (OnChanged)
	{
		OnChanged(Value, Max);
	}
}
