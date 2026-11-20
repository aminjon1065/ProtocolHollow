#pragma once

#include "GameplayEffect.h"

#include "HollowDeadEffect.generated.h"

/**
 * Structure of the death state (ADR-0016): infinite, grants Hollow.State.Dead (blocks interaction; for AI a body) and
 * blocks tools and gadgets. GE_State_Dead derives from this; the dead stay dead until their next pawn (a new player state).
 */
UCLASS(Abstract)
class HOLLOWGAS_API UHollowDeadEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit UHollowDeadEffect(const FObjectInitializer& ObjectInitializer);
};
