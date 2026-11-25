#include "Effects/HollowSwarmEffects.h"

#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSwarmEffects)

UHollowLitEffect::UHollowLitEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitRenewedStructure(HollowTags::State_Lit);
}

UHollowFrenzyEffect::UHollowFrenzyEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitRenewedStructure(HollowTags::State_Frenzy);
}
