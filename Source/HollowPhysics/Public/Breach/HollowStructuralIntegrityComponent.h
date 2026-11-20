#pragma once

#include "Breach/HollowBreachRules.h"
#include "Components/ActorComponent.h"

#include "HollowStructuralIntegrityComponent.generated.h"

class UHollowBreachableDefinition;

/**
 * Replicated integrity of a breachable object (Docs/Architecture/03 sec. 3.4). Changed only on the server through
 * HollowBreachRules; every machine follows NetState (push model; the owner is dormant between changes). Zone health
 * stays on the server, clients see the damage stage and the broken zones.
 */
UCLASS(ClassGroup = (Hollow), meta = (BlueprintSpawnableComponent))
class HOLLOWPHYSICS_API UHollowStructuralIntegrityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Old and new replicated state, on every machine (the server right after a change, clients on replication). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowBreachStateChanged, const FHollowBreachNetState& /*Old*/, const FHollowBreachNetState& /*New*/);

	explicit UHollowStructuralIntegrityComponent(const FObjectInitializer& ObjectInitializer);

	const UHollowBreachableDefinition* GetDefinition() const { return Definition; }
	const FHollowBreachNetState& GetNetState() const { return NetState; }
	/** Server time, the clock of every break. */
	double GetServerTime() const;
	bool CanTakeDamage() const;

	/** Server only: exact health of a zone. */
	float GetZoneHealth(int32 Zone) const;

	/** Server only. */
	EHollowBreachOutcome ApplyHit(const FVector& WorldLocation, float Amount, const FVector& WorldDirection);

	FOnHollowBreachStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = "Hollow|Breach")
	TObjectPtr<UHollowBreachableDefinition> Definition;

private:
	UFUNCTION()
	void OnRep_NetState(const FHollowBreachNetState& OldState);

	UPROPERTY(ReplicatedUsing = OnRep_NetState)
	FHollowBreachNetState NetState;

	/** Server: exact zone health behind NetState. */
	TArray<float, TInlineAllocator<FHollowBreachNetState::MaxZones>> Health;
};
