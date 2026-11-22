#pragma once

#include "GameFramework/Actor.h"
#include "World/HollowArcRules.h"

#include "HollowArcHazard.generated.h"

class AHollowSwitch;
class APawn;
class UHollowArcHazardDefinition;
class UInstancedStaticMeshComponent;
class UPointLightComponent;

/** What every machine knows of an arc (push model, ADR-0039): power, and the server time its cycle began. */
USTRUCT()
struct FHollowArcNetState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bPowered = false;

	UPROPERTY()
	double CycleStartServerTime = 0.0;
};

/**
 * An electric arc across a passage of the Reactor Cathedral (S7-02, ADR-0039), spawned by the server at an ArcHazard
 * socket. Powered, it cycles: a warning (the emitters crackle), a strike (the arcs flash; every living player inside
 * when it starts takes DamageEffect), a rest, again. Its breaker, an AHollowSwitch it spawns beside the passage, cuts
 * the power: the cycle stops at once; switched on again it starts over with the warning. Every machine shows the cycle
 * from the replicated power and cycle start (timers, no Tick, no RPC); only the server strikes.
 */
UCLASS()
class HOLLOWGAME_API AHollowArcHazard : public AActor
{
	GENERATED_BODY()

public:
	explicit AHollowArcHazard(const FObjectInitializer& ObjectInitializer);

	const UHollowArcHazardDefinition* GetDefinition() const { return Definition; }
	bool IsPowered() const { return ArcState.bPowered; }
	/** The phase on this machine's clock now. */
	FHollowArcMoment GetMoment() const;
	/** Server: its breaker (spawned in BeginPlay). */
	AHollowSwitch* GetBreaker() const { return Breaker; }
	/** Inside the zone: the box ZoneExtent in the arc's frame, standing on the floor. */
	bool Contains(const FVector& Point) const;
	/** Server: strikes now: damage to every living player inside (the cycle's; tests call it). Returns how many it hit. */
	int32 Strike();
	/** Server: strikes so far. */
	int32 GetStrikes() const { return Strikes; }
	/** Server: power on (a new cycle, warning first) or off (the breaker's). */
	void SetPowered(bool bInPowered);
	/** One line for Hollow.Arc.Dump and the smoke logs. */
	FString Describe() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Arc")
	TObjectPtr<USceneComponent> Root;

	/** The emitters on the walls (UHollowArcHazardDefinition::Emitters). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Arc")
	TObjectPtr<UInstancedStaticMeshComponent> Emitters;

	/** The arcs (UHollowArcHazardDefinition::Beams), shown while it strikes. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Arc")
	TObjectPtr<UInstancedStaticMeshComponent> Beams;

	/** Shadowless light over the passage: crackles during the warning, flashes with the strike. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Arc")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Arc")
	TObjectPtr<const UHollowArcHazardDefinition> Definition;

private:
	double ServerNow() const;
	/** Every machine: the look of the current phase, a timer to the next one; the server strikes on entering a strike. */
	void ApplyPhase();
	/** Every machine, during the warning: the light on, off, on... */
	void Crackle();
	/** Server: the breaker was switched. */
	void HandleBreakerToggled(bool bIsOn);

	UFUNCTION()
	void OnRep_ArcState();

	UPROPERTY(ReplicatedUsing = OnRep_ArcState)
	FHollowArcNetState ArcState;

	/** Server: the switch that powers it. */
	UPROPERTY(Transient)
	TObjectPtr<AHollowSwitch> Breaker;

	EHollowArcPhase ShownPhase = EHollowArcPhase::Off;
	bool bCrackleLit = false;
	float GlowIntensity = 0.f;
	/** Server: the cycle that already struck (one strike a cycle). */
	int64 StruckCycle = INDEX_NONE;
	int32 Strikes = 0;

	FTimerHandle PhaseTimer;
	FTimerHandle CrackleTimer;
};
