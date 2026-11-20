#pragma once

#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "HollowStructural.generated.h"

class AActor;

/** One structural blow: Iron Pilgrim, heavy hammer, explosion (Docs/Architecture/01 interfaces, 03 sec. 3.4). */
USTRUCT(BlueprintType)
struct FHollowStructuralHit
{
	GENERATED_BODY()

	/** Structural damage points; the target's data says how much it can take. */
	UPROPERTY(BlueprintReadWrite, Category = "Structural")
	float Amount = 0.f;

	/** World point of the blow: the target picks the part it lands on. */
	UPROPERTY(BlueprintReadWrite, Category = "Structural")
	FVector Location = FVector::ZeroVector;

	/** Direction the blow travels (debris flies along it); zero: from the instigator towards the target. */
	UPROPERTY(BlueprintReadWrite, Category = "Structural")
	FVector Direction = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Structural", meta = (Categories = "Hollow.Damage"))
	FGameplayTag DamageType;

	UPROPERTY(BlueprintReadWrite, Category = "Structural")
	TWeakObjectPtr<AActor> Instigator;
};

/** What it takes an enemy to get past a structural obstacle on its path (Iron Pilgrim, S3-04, ADR-0017). */
UENUM(BlueprintType)
enum class EHollowPassage : uint8
{
	/** Nothing in the way. */
	Clear,
	/** Opens like a door (IHollowInteractable), or is about to be passable (a leaf still moving): open it or wait. */
	Open,
	/** Only structural damage gets through: welded or locked door, intact bulkhead. */
	Breach
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowStructural : public UInterface
{
	GENERATED_BODY()
};

/**
 * A world object that breaks under structural damage: bulkheads now, doors and barricades with the Iron Pilgrim (S3).
 * Integrity is replicated component state changed only here, on the server (CLAUDE.md sec. 3, Q4: no ASC).
 */
class HOLLOWCORE_API IHollowStructural
{
	GENERATED_BODY()

public:
	/** Can it still be damaged? Any machine, from replicated state. */
	virtual bool CanTakeStructuralDamage() const = 0;

	/** Server only. True if the blow changed anything. */
	virtual bool ApplyStructuralDamage(const FHollowStructuralHit& Hit) = 0;

	/** How to get past it right now, from replicated state (any machine). */
	virtual EHollowPassage GetPassage() const = 0;
};
