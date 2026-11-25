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

/** Iron Pilgrim's melee strike (Hollow.Event.Enemy.Melee): the shared melee sweep with its own attack data. */
UCLASS()
class HOLLOWAI_API UHollowGA_PilgrimMelee : public UHollowGA_EnemyMelee
{
	GENERATED_BODY()

public:
	explicit UHollowGA_PilgrimMelee(const FObjectInitializer& ObjectInitializer);
};

/**
 * Echo-Stalker's strike out of an ambush (Hollow.Event.Enemy.Ambush, ADR-0019): the shared melee sweep, a short
 * wind-up, occult damage that armor does not stop (its attack data).
 */
UCLASS()
class HOLLOWAI_API UHollowGA_StalkerAmbush : public UHollowGA_EnemyMelee
{
	GENERATED_BODY()

public:
	explicit UHollowGA_StalkerAmbush(const FObjectInitializer& ObjectInitializer);
};

/**
 * Whisperer's bite (Hollow.Event.Enemy.Melee, GP-02 ADR-0047): the shared melee sweep with its own attack data, a short
 * wind-up (the small glow of GCN_Whisperer_Windup, its chitter), little damage: the swarm bites many times.
 */
UCLASS()
class HOLLOWAI_API UHollowGA_WhispererBite : public UHollowGA_EnemyMelee
{
	GENERATED_BODY()

public:
	explicit UHollowGA_WhispererBite(const FObjectInitializer& ObjectInitializer);
};
