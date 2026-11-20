#pragma once

#include "Components/ActorComponent.h"
#include "Resonance/HollowResonanceTypes.h"

#include "HollowSquadResonanceComponent.generated.h"

class UHollowResonanceConfig;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowSquadResonanceChanged, const FHollowSquadResonanceNetState& /*State*/);

/**
 * The squad's Resonance on the game state (ADR-0020). The server aggregates the living players' Resonance every
 * UpdateSeconds (the highest of them), quantizes it to a byte, keeps the tier with hysteresis and replicates both
 * (push model, on change only). Every machine hears OnSquadResonanceChanged: the server when it changes, clients on
 * replication. Visuals, sound and the AI read it; nothing here ticks.
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

	UFUNCTION()
	void OnRep_State(const FHollowSquadResonanceNetState& OldState);

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FHollowSquadResonanceNetState State;

	UPROPERTY(Transient)
	TObjectPtr<const UHollowResonanceConfig> Config;

	FTimerHandle UpdateTimer;
};
