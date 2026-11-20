#pragma once

#include "AttributeSet.h"
#include "Templates/Function.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 * Follows an attribute and its maximum on an ability system and calls back with both on every change (visor bars,
 * no Tick). Replicated attributes notify through their OnRep, so clients update when values arrive.
 */
class HOLLOWUI_API FHollowAttributeWatch : public FNoncopyable
{
public:
	using FOnChanged = TFunction<void(float /*Value*/, float /*Max*/)>;

	~FHollowAttributeWatch();

	/** Unbinds any previous source, binds this one and calls back once with the current values. */
	void Bind(UAbilitySystemComponent* InAbilitySystem, const FGameplayAttribute& InAttribute, const FGameplayAttribute& InMaxAttribute, FOnChanged InOnChanged);
	void Unbind();

	bool IsBound() const { return AbilitySystem.IsValid(); }
	float GetValue() const { return Value; }
	float GetMax() const { return Max; }
	float GetRatio() const { return Ratio(Value, Max); }

	/** Value / Max clamped to 0..1; 0 without a positive maximum. */
	static float Ratio(float InValue, float InMax);

private:
	void HandleChange(const FOnAttributeChangeData& Change);
	void Refresh();

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FGameplayAttribute Attribute;
	FGameplayAttribute MaxAttribute;
	FDelegateHandle ValueHandle;
	FDelegateHandle MaxHandle;
	FOnChanged OnChanged;
	float Value = 0.f;
	float Max = 0.f;
};
