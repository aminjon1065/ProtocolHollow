#pragma once

#include "Collision/HollowCollisionChannels.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowWeaponDefinition.generated.h"

class UFXSystemAsset;
class UGameplayEffect;
class UStaticMesh;

/**
 * Stats of a weapon (GP-01, ADR-0044), primary asset type HollowWeapon: how it fires, what a hit does, its magazine and
 * its looks. Everything the weapon abilities do is driven from here (CLAUDE.md sec. 1: data, not code). The magazine is
 * UHollowWeaponSet's (an attribute, like the tools' fuel); the rounds behind it are AmmoItem in the raid inventory.
 */
UCLASS(BlueprintType)
class HOLLOWGAS_API UHollowWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Damage of one pellet at Distance from the muzzle: full up to FalloffStartRange, MinDamageFraction from FalloffEndRange. */
	float GetPelletDamage(float Distance) const;

	/** The spread's half-angle, tighter while aimed. */
	float GetSpreadDegrees(bool bAiming) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "Hollow.Weapon"))
	FGameplayTag WeaponTag;

	/** What the visor calls it ("NAILGUN"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	/** The owner fires at most once per this many seconds; the server's floor is the fire ability's cooldown effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "0.01", Units = "Seconds"))
	float FireIntervalSeconds = 0.f;

	/** Pellets of one shot (1: a single round). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "1", ClampMax = "16"))
	int32 Pellets = 1;

	/** Half-angle of the cone the pellets leave in, from the hip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "0", ClampMax = "30", Units = "Degrees"))
	float SpreadDegrees = 0.f;

	/** The spread while aimed (secondary input), as a share of SpreadDegrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "0", ClampMax = "1"))
	float AimSpreadMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "1", Units = "Centimeters"))
	float Range = 0.f;

	/** Channel the shots trace on (HollowCollision::Weapon). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire")
	TEnumAsByte<ECollisionChannel> TraceChannel = HollowCollision::Weapon;

	/** Base damage of one pellet before the target's armour (SetByCaller Hollow.Data.Damage of DamageEffect). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0"))
	float DamagePerPellet = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0", Units = "Centimeters"))
	float FalloffStartRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0", Units = "Centimeters"))
	float FalloffEndRange = 0.f;

	/** Share of the damage left from FalloffEndRange on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0", ClampMax = "1"))
	float MinDamageFraction = 1.f;

	/** Share of the target's armour a hit ignores (SetByCaller Hollow.Data.ArmorPenetration). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0", ClampMax = "1"))
	float ArmorPenetration = 0.f;

	/** More of it while the target is exposed by an Aether lantern (Hollow.Data.ExposedPenetration). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0", ClampMax = "1"))
	float ExposedPenetrationBonus = 0.f;

	/** Instant damage effect through UHollowDamageExecution (GE_Damage_Kinetic). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Push of one pellet on what it hits (cm/s before the target's own resistance, IHollowImpulseReceiver). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float ImpulsePerPellet = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "1"))
	int32 MagazineSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "1"))
	int32 AmmoPerShot = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0", Units = "Seconds"))
	float ReloadSeconds = 0.f;

	/** The rounds a reload takes from the raid inventory (a consumable item). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId AmmoItem;

	/** Server check of the owner's hits (ADR-0044): its aim may differ from the server's view by this much... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Server Check", meta = (ClampMin = "0", Units = "Degrees"))
	float AimToleranceDegrees = 0.f;

	/** ...and its eyes be this far from where the server sees them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Server Check", meta = (ClampMin = "0", Units = "Centimeters"))
	float OriginToleranceCm = 0.f;

	/** A hit may land this far around the target's bounds (on top of its movement during the owner's latency). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Server Check", meta = (ClampMin = "0", Units = "Centimeters"))
	float BoundsToleranceCm = 0.f;

	/** Hollow.Noise.* of one shot at the shooter; loudness and range are its rule in DA_NoiseConfig (ADR-0018). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (Categories = "Hollow.Noise"))
	FGameplayTag NoiseTag;

	/** GP-01c (ADR-0046): what a shot feels like to its shooter (kick, shake, rumble), its entry in DA_FeelConfig. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Feel", meta = (Categories = "Hollow.Feel"))
	FGameplayTag FeelTag;

	/** The weapon in hand: shown by UHollowToolMountComponent like a tool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Held")
	TSoftObjectPtr<UStaticMesh> HeldMesh;

	/** The held mesh relative to the hand's socket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Held")
	FTransform HeldTransform = FTransform::Identity;

	/** Socket of HeldMesh the shots leave from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Held")
	FName MuzzleSocket = TEXT("Muzzle");

	/** Where the first-person weapon sits in the camera's space while it rides the camera (ADR-0043). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Held")
	FTransform ViewOffset = FTransform::Identity;

	/** Flash at the muzzle, once per shot (a Niagara system). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Shot")
	TSoftObjectPtr<UFXSystemAsset> MuzzleFlash;

	/** One per pellet, from the muzzle to where it lands (a Niagara system with a user vector "End"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Shot")
	TSoftObjectPtr<UFXSystemAsset> Tracer;

	/** Where a pellet lands, along the surface normal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics|Shot")
	TSoftObjectPtr<UFXSystemAsset> Impact;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
