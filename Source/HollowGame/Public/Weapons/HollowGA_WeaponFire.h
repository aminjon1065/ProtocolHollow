#pragma once

#include "Abilities/HollowGameplayAbility.h"

#include "HollowGA_WeaponFire.generated.h"

class AActor;
class APawn;
class UHollowHandsComponent;
class UHollowWeaponDefinition;
struct FGameplayAbilityTargetDataHandle;
struct FHollowWeaponShotData;

/**
 * One shot of a weapon (GP-01, ADR-0044), granted by the weapon's item on the primary input; needs the weapon in hand.
 * The owner traces its pellets at once (its own eyes and aim, the spread seeded by the shot's serial), shows the shot
 * (predicted cue) and sends what it hit as target data; the server checks every hit against its own view of the
 * shooter (HollowWeaponMath::CheckHit), then deals the damage (DamageEffect through UHollowDamageExecution: the
 * weapon's armour penetration, more on exposed targets), pushes what it hit (IHollowImpulseReceiver), tells the enemies
 * (a noise at the shooter, the damage sense) and runs the cue for everyone else. The listen-server host's own hits
 * are applied as they are. Costs one round of the magazine (CostGameplayEffectClass, predicted); the server's floor
 * under the rate of fire is its cooldown effect, the owner keeps the weapon's exact interval.
 */
UCLASS()
class HOLLOWGAME_API UHollowGA_WeaponFire : public UHollowGameplayAbility
{
	GENERATED_BODY()

public:
	explicit UHollowGA_WeaponFire(const FObjectInitializer& ObjectInitializer);

	const UHollowWeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }

	/**
	 * The pellets of a shot: from Origin along Aim inside the weapon's spread (tighter aimed), seeded by Serial, traced on
	 * the weapon's channel up to its range. One result per pellet (no blocking hit: it flew its range, TraceEnd). The
	 * owner's shot and every machine's tracers come from here.
	 */
	static void TracePellets(const UWorld& World, const UHollowWeaponDefinition& Weapon, const APawn* Shooter, const FVector& Origin,
		const FVector& Aim, uint16 Serial, bool bAiming, TArray<FHitResult>& OutPellets);

	/** The pellets that hit an actor, as the owner sends them. */
	static void MakeShotData(const TArray<FHitResult>& Pellets, const FVector& Origin, const FVector& Aim, uint16 Serial, bool bAiming,
		FHollowWeaponShotData& OutShot);

	/**
	 * Server: what a shot did. bTrusted (the listen-server host's own shot) skips the checks. Returns the hits it took.
	 * Public for the automation tests.
	 */
	int32 ApplyShot(const FHollowWeaponShotData& Shot, bool bTrusted);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Weapon")
	TObjectPtr<const UHollowWeaponDefinition> WeaponDefinition;

private:
	/** Server: the owner's shot arrived. */
	void HandleShotData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);

	/** The shot's cue on this machine (predicted on the owner, from the server for everyone else). */
	void ExecuteShotCue(const FVector& Origin, const FVector& Aim, uint16 Serial, bool bAiming);

	/** Server: the shooter's noise for the AI (its rule limits nothing: every shot is heard). */
	void ReportShotNoise(APawn& Shooter) const;

	/** Owner (GP-01c): the shot's feel (kick, shake, rumble) and the hit marker of what its pellets hit, as it sees them. */
	void ShowShotFeel(const APawn& Shooter, UHollowHandsComponent& Hands, const TArray<FHitResult>& Pellets, const FVector& Origin) const;

	FDelegateHandle ShotDataHandle;
	/** The activation the server waits for the owner's shot of. */
	FPredictionKey WaitingKey;
};
