#pragma once

#include "Abilities/HollowGameplayAbility.h"

#include "HollowGA_MeleeBash.generated.h"

class UGameplayEffect;

/**
 * A quick blow with what is in hand (GP-01, ADR-0044), in both hand modes: the suit's ability on the melee input (Q, the
 * gamepad's right stick). The server sweeps a short sphere from its view of the player (short and wide: no hit of the
 * owner's is needed) and the first thing struck takes the damage (DamageEffect, Hollow.Data.Damage) and a push
 * (IHollowImpulseReceiver), with a noise there for the AI. Its cooldown is data; squad mates are never hurt.
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_MeleeBash : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_MeleeBash(const FObjectInitializer& ObjectInitializer);

	/** Server: the blow from Origin along Aim; returns what it struck (null: nothing). Public for the automation tests. */
	AActor* Strike(const FVector& Origin, const FVector& Aim);

	const FGameplayTag& GetFeelTag() const { return FeelTag; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Melee", meta = (ClampMin = "0"))
	float Damage = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Melee", meta = (ClampMin = "0", Units = "Centimeters"))
	float Range = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Melee", meta = (ClampMin = "0", Units = "Centimeters"))
	float Radius = 0.f;

	/** The push on what it strikes (cm/s before its own resistance). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Melee", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float Impulse = 0.f;

	/** Instant damage effect through UHollowDamageExecution (GE_Damage_Kinetic). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Melee")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Hollow.Noise.* where a blow lands (its rule in DA_NoiseConfig). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Melee", meta = (Categories = "Hollow.Noise"))
	FGameplayTag NoiseTag;

	/** GP-01c (ADR-0046): what a blow feels like to the striker (DA_FeelConfig). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Melee", meta = (Categories = "Hollow.Feel"))
	FGameplayTag FeelTag;

private:
	/** What a blow from Origin along Aim meets (the server's strike and the owner's marker sweep the same). */
	bool Sweep(const APawn& Pawn, const FVector& Origin, const FVector& Aim, FHitResult& OutHit) const;
};
