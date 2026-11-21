#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

#include "HollowEnemyMontages.generated.h"

class UAnimMontage;

/** The moments of an enemy a montage can show (ADR-0027). */
UENUM(BlueprintType)
enum class EHollowEnemyMontage : uint8
{
	/** Wind-up and blow of a melee attack (the Pilgrim's swing, the Echo-Stalker's strike out of an ambush). */
	Melee,
	/** Wind-up and blow at a door or a bulkhead. */
	Breach,
	/** Stunned by a pulse mine: plays while Hollow.State.Stunned lasts (loop a section). */
	Stunned,
	/** Into a vent: the body hides once it has played. */
	VentEnter,
	/** Out of a vent. */
	VentExit
};

/**
 * Optional montages of an enemy's moments (UHollowEnemyDefinition::Montages, art pass). Presentation only: gameplay
 * timing stays in the data (wind-up seconds, stun duration, vent travel) and no root motion moves the body. An empty
 * slot keeps the greybox behaviour.
 */
USTRUCT(BlueprintType)
struct HOLLOWAI_API FHollowEnemyMontages
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	TSoftObjectPtr<UAnimMontage> Melee;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	TSoftObjectPtr<UAnimMontage> Breach;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	TSoftObjectPtr<UAnimMontage> Stunned;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	TSoftObjectPtr<UAnimMontage> VentEnter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	TSoftObjectPtr<UAnimMontage> VentExit;

	const TSoftObjectPtr<UAnimMontage>& Get(EHollowEnemyMontage Moment) const;

	/** Every assigned slot. */
	TArray<FSoftObjectPath> Assigned() const;
};

namespace HollowEnemyMontages
{
	/** A fitted montage plays no slower or faster than this (beyond it the wind-up data and the art disagree too much). */
	constexpr float MinPlayRate = 0.25f;
	constexpr float MaxPlayRate = 4.f;

	/**
	 * Play rate that brings ImpactSection's start to Seconds after the start (the blow lands when the attack hits);
	 * 1 without the section, without a positive time, or for Seconds <= 0. Clamped to MinPlayRate..MaxPlayRate.
	 */
	HOLLOWAI_API float PlayRateToLand(const UAnimMontage& Montage, FName ImpactSection, float Seconds);
}
