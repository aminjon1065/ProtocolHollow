#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "HollowAttributeSetBase.generated.h"

/** A current attribute and the max attribute bounding it (e.g. Health / MaxHealth). */
struct FHollowBoundedAttribute
{
	FGameplayAttribute Current;
	FGameplayAttribute Max;
};

/**
 * Base of every Hollow attribute set (CLAUDE.md sec. 4). Values change only through gameplay effects; the set keeps
 * them valid: bounded attributes stay in [0, Max], other non-meta attributes stay >= 0, and lowering a max pulls its
 * current attribute down with it (through the ability system, never a raw write).
 * Accessors use the engine's ATTRIBUTE_ACCESSORS_BASIC.
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	/** Current/max pairs of this set. */
	virtual TConstArrayView<FHollowBoundedAttribute> GetBoundedAttributes() const { return {}; }

	/** Meta attributes are transient server-side inputs (e.g. IncomingDamage) and are never clamped. */
	virtual bool IsMetaAttribute(const FGameplayAttribute& Attribute) const { return false; }

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

protected:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
