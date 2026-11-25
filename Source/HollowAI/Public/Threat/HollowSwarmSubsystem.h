#pragma once

#include "Interfaces/HollowLightSource.h"
#include "Subsystems/WorldSubsystem.h"

#include "HollowSwarmSubsystem.generated.h"

class AHollowEnemyCharacter;
class AHollowWhispererController;
class UHollowThreatConfig;

/**
 * The Whisperer swarms of a sector (GP-02, ADR-0047), server only. Every Whisperer's brain registers here; a nest's
 * brood is one group. On one timer (UHollowThreatConfig::Swarm.LightCheckSeconds, only while Whisperers live) it gathers
 * the players' light cones once (IHollowLightSource: lamps, burning lanterns) and checks each Whisperer: a cone that
 * reaches it in a clear line renews its LitEffect (it flinches and backs away, slowed) and tells its brain where the
 * light came from; in the dark, with prey whose light is off, it renews its FrenzyEffect. It also keeps the bite slots
 * (at most MaxBitersPerPrey bite one lit player at a time, a frenzy bites freely) and passes a sighting to the rest of
 * the group. Nothing of this is replicated: the effects' tags reach clients with the enemy's ability system.
 */
UCLASS()
class HOLLOWAI_API UHollowSwarmSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowSwarmSubsystem* Get(const UObject* WorldContext);

	/** Its brain on possession (the light check runs while any is registered). */
	void Register(AHollowWhispererController& Whisperer);
	void Unregister(const AHollowWhispererController& Whisperer);

	/** A nest's brood: one group, whose members tell each other what they see. */
	void RegisterBrood(TConstArrayView<AHollowEnemyCharacter*> Brood);

	/** Whisperer has just found Prey: the others of its group hear where Prey is and come. */
	void ReportSighting(const AHollowWhispererController& Whisperer, const APawn& Prey);

	/** Whether Whisperer may start a bite at Prey now (HollowSwarmRules::CanBite over the others holding a slot on it). */
	bool CanBite(const AHollowWhispererController& Whisperer, const APawn* Prey) const;
	/** Its bite at Prey starts: it holds a slot until ReleaseBiteSlot. */
	void TakeBiteSlot(const AHollowWhispererController& Whisperer, APawn* Prey);
	void ReleaseBiteSlot(const AHollowWhispererController& Whisperer);
	/** Whisperers other than Except holding a bite slot on Prey. */
	int32 CountBiters(const APawn* Prey, const AHollowWhispererController* Except = nullptr) const;

	/** The light check now (the timer runs it; tests run it themselves). */
	void CheckLight();

	/**
	 * The first of Cones that reaches Point in a clear line (Visibility, the cone's caster and Ignored left out), or none:
	 * the dark.
	 */
	static bool FindLightOn(const UWorld& World, const FVector& Point, const AActor* Ignored, TConstArrayView<TPair<FHollowLightCone, const AActor*>> Cones,
		FHollowLightCone& OutCone);

	/** Whether Pawn casts any light now (IHollowLightSource). */
	static bool HasLight(const APawn* Pawn);

	int32 GetWhispererCount() const { return Members.Num(); }
	int32 GetChecks() const { return Checks; }
	/** Whisperers lit / frenzied at the last check. */
	int32 GetLitCount() const { return LastLit; }
	int32 GetFrenziedCount() const { return LastFrenzied; }
	/** Group of Whisperer's pawn (INDEX_NONE: none, spawned on its own). */
	int32 GetGroupOf(const APawn* Whisperer) const;

	/** Tuning in use (UHollowThreatConfig::Get by default); tests swap in their own. */
	void SetConfig(const UHollowThreatConfig* InConfig);

	/** One line for Hollow.AI.Swarm and the smoke logs. */
	FString Describe() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	struct FMember
	{
		TWeakObjectPtr<AHollowWhispererController> Brain;
		TWeakObjectPtr<APawn> Body;
		int32 Group = INDEX_NONE;
		TWeakObjectPtr<APawn> BitePrey;
	};

	FMember* Find(const AHollowWhispererController& Whisperer);
	const FMember* Find(const AHollowWhispererController& Whisperer) const;
	void UpdateTimer();
	void Prune();

	UPROPERTY(Transient)
	TObjectPtr<const UHollowThreatConfig> Config;

	TArray<FMember> Members;
	/** Group of each pawn of a registered brood (a brood may register before or after its brains). */
	TArray<TPair<TWeakObjectPtr<APawn>, int32>> Groups;
	int32 NextGroup = 0;
	int32 Checks = 0;
	int32 LastLit = 0;
	int32 LastFrenzied = 0;
	FTimerHandle LightTimer;
};
