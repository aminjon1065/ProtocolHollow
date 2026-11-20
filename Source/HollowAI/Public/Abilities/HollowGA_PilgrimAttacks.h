#pragma once

#include "Abilities/HollowGA_EnemyAttack.h"

#include "HollowGA_PilgrimAttacks.generated.h"

/**
 * Iron Pilgrim's structural blow (Hollow.Event.Enemy.Breach): IHollowStructural::ApplyStructuralDamage on the door or
 * bulkhead in its way, if within reach. The obstacle's own rules decide what the blow does (ADR-0014, ADR-0017).
 */
UCLASS()
class HOLLOWAI_API UHollowGA_PilgrimBreach : public UHollowGA_EnemyAttack
{
	GENERATED_BODY()

public:
	explicit UHollowGA_PilgrimBreach(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool CanStrike(const AActor* Target) const override;
	virtual void Strike(AActor* Target) override;
};

/**
 * Iron Pilgrim's melee strike (Hollow.Event.Enemy.Melee): a sphere sweep in front of it at the end of the wind-up;
 * every living player it catches in line of sight takes the definition's damage effect (UHollowDamageExecution).
 * Whoever got out of the way in time is not hit, the target included.
 */
UCLASS()
class HOLLOWAI_API UHollowGA_PilgrimMelee : public UHollowGA_EnemyAttack
{
	GENERATED_BODY()

public:
	explicit UHollowGA_PilgrimMelee(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool CanStrike(const AActor* Target) const override;
	virtual void Strike(AActor* Target) override;
};
