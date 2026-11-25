#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/NetSerialization.h"

#include "HollowWeaponShotData.generated.h"

class AActor;

/** One pellet of a shot that hit something (GP-01). */
USTRUCT()
struct FHollowPelletHit
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY()
	FVector_NetQuantize Impact;

	UPROPERTY()
	FVector_NetQuantizeNormal Normal;

	/** Which pellet of the shot (its direction comes from the shot's serial). */
	UPROPERTY()
	uint8 Pellet = 0;
};

/**
 * What the owner saw of one shot (GP-01, ADR-0044), sent to the server as the fire ability's target data: its eyes, aim
 * and serial, and the pellets that hit an actor (the server checks every one, HollowWeaponMath::CheckHit). Compact:
 * a shotgun blast with every pellet on target is about 150 bytes.
 */
USTRUCT()
struct HOLLOWGAME_API FHollowWeaponShotData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	/** At most this many hits are sent (the weapon's pellets are clamped to it). */
	static constexpr int32 MaxHits = 16;

	UPROPERTY()
	FVector_NetQuantize10 Origin;

	UPROPERTY()
	FVector_NetQuantizeNormal Aim;

	UPROPERTY()
	uint16 Serial = 0;

	UPROPERTY()
	bool bAiming = false;

	UPROPERTY()
	TArray<FHollowPelletHit> Hits;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FString ToString() const override;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template <>
struct TStructOpsTypeTraits<FHollowWeaponShotData> : public TStructOpsTypeTraitsBase2<FHollowWeaponShotData>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
