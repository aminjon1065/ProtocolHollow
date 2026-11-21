#pragma once

#include "Components/ActorComponent.h"
#include "Resonance/HollowResonanceTypes.h"

#include "HollowSquadResonanceComponent.generated.h"

class APlayerState;
class UAbilitySystemComponent;
class UHollowResonanceConfig;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowSquadResonanceChanged, const FHollowSquadResonanceNetState& /*State*/);

/**
 * The squad's Resonance on the game state (ADR-0020). The server aggregates the living players' Resonance every
 * UpdateSeconds (the highest of them), quantizes it to a byte, keeps the tier with hysteresis and replicates both
 * (push model, on change only). Every machine hears OnSquadResonanceChanged: the server when it changes, clients on
 * replication. Visuals, sound and the AI read it; nothing here ticks.
 * Every player's ability system carries exactly one Hollow.State.Resonance.<Tier> tag for the squad's tier (S4-08):
 * each machine mirrors the replicated tier as loose tags (nothing more on the wire, the same moment as the visuals).
 */
UCLASS(ClassGroup = (Hollow), meta = (BlueprintSpawnableComponent))
class HOLLOWGAS_API UHollowSquadResonanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit UHollowSquadResonanceComponent(const FObjectInitializer& ObjectInitializer);

	/** The component of the world's game state, or null. */
	static UHollowSquadResonanceComponent* Get(const UObject* WorldContext);

	const FHollowSquadResonanceNetState& GetState() const { return State; }
	float GetNormalized() const;
	EHollowResonanceTier GetTier() const { return State.Tier; }

	/** Server: aggregates the squad now (the timer's step; tests call it directly). */
	void Evaluate();

	/** Puts the squad's tier tag (and only it) on a player's ability system; the game state calls it for every new player. */
	void MirrorTierTo(const APlayerState* Player) const;
	static void MirrorTier(UAbilitySystemComponent& AbilitySystem, EHollowResonanceTier Tier);

	/** Tuning in use (UHollowResonanceConfig::Get by default); tests swap in their own. */
	void SetConfig(const UHollowResonanceConfig* InConfig);

	FString Describe() const;

	FOnHollowSquadResonanceChanged OnSquadResonanceChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void StartTimer();
	/** The tier tag on every player of the game state. */
	void MirrorTierToSquad() const;

	UFUNCTION()
	void OnRep_State(const FHollowSquadResonanceNetState& OldState);

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FHollowSquadResonanceNetState State;

	UPROPERTY(Transient)
	TObjectPtr<const UHollowResonanceConfig> Config;

	FTimerHandle UpdateTimer;
};
