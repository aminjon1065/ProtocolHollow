#pragma once

#include "Interfaces/HollowLightSource.h"
#include "Tools/HollowGA_ContinuousTool.h"

#include "HollowGA_AetherLantern.generated.h"

class AHollowEnemyCharacter;
class UAbilitySystemComponent;
class UGameplayEffect;
class UHollowLanternDefinition;

/**
 * The Techno-Occultist's Aether lantern (S6-02, ADR-0035): a continuous tool (fuel, heat, overheat, quiet hum, the
 * light cone cue on every machine) whose server samples light a cone instead of working on a target. Every sample
 * renews ExposeEffect on each enemy in the cone and in sight (slowed, weaker armour, the Echo-Stalker flees) and
 * AuraEffect on each living player within AuraRadius of the wielder, the wielder too (Resonance builds slower). The
 * effects are short and linger once the light or the aura has gone; their tags and the glow replicate with them.
 */
UCLASS(Abstract)
class HOLLOWGAME_API UHollowGA_AetherLantern : public UHollowGA_ContinuousTool
{
	GENERATED_BODY()

public:
	explicit UHollowGA_AetherLantern(const FObjectInitializer& ObjectInitializer);

	const UHollowLanternDefinition* GetLanternDefinition() const;

	/** The living enemies out of the vents in the lantern's cone from Eyes along Direction and in its sight, by name. */
	static TArray<AHollowEnemyCharacter*> FindLitEnemies(const UWorld& World, const APawn& Wielder, const FVector& Eyes, const FVector& Direction,
		const UHollowLanternDefinition& Lantern);

	/** The living players within the lantern's aura around Wielder (Wielder too), by name. */
	static TArray<APawn*> FindAuraPlayers(const UWorld& World, const APawn& Wielder, const UHollowLanternDefinition& Lantern);

	/** The lantern's light cone from Eyes along Direction (Kind Lantern). */
	static FHollowLightCone MakeCone(const FVector& Eyes, const FVector& Direction, const UHollowLanternDefinition& Lantern);

	/**
	 * The beam of Wielder's lantern while it burns, from where Wielder looks (GP-02: the AI's light, IHollowLightSource).
	 * Where the ability runs: the server (the AI's truth) and the predicting owner.
	 */
	static bool FindBeamCone(const APawn& Wielder, FHollowLightCone& OutCone);

protected:
	virtual void ApplySample(APawn& Wielder, const FVector& Eyes, const FVector& Direction, float DeltaSeconds) override;
	virtual FGameplayTag GetCueTag() const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	bool ApplyTo(UAbilitySystemComponent* Target, const TSubclassOf<UGameplayEffect>& Effect) const;

	/** Who the light and the aura reached at the last sample (logged when it changes). */
	FString LastLit;
	FString LastAura;
};
