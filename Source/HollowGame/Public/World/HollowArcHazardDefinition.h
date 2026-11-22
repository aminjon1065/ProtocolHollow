#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "World/HollowArcRules.h"

#include "HollowArcHazardDefinition.generated.h"

class AHollowSwitch;
class UGameplayEffect;

/**
 * Data of an electric arc of the Reactor Cathedral (S7-02, ADR-0039): its cycle, its zone and damage, its breaker and
 * its greybox shape. AHollowArcHazard only runs it.
 */
UCLASS(BlueprintType)
class HOLLOWGAME_API UHollowArcHazardDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** The warning before every strike. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cycle", meta = (ClampMin = "0.1", Units = "Seconds"))
	float TelegraphSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cycle", meta = (ClampMin = "0.1", Units = "Seconds"))
	float DischargeSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cycle", meta = (ClampMin = "0.1", Units = "Seconds"))
	float IdleSeconds = 0.f;

	/** Half size of the zone in the arc's frame (X along the passage, Y across it, Z up from its centre). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	FVector ZoneExtent = FVector::ZeroVector;

	/** Instant damage applied to each living player inside when a strike starts (SetByCaller Hollow.Data.Damage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0"))
	float Damage = 0.f;

	/** The switch that powers it (an AHollowSwitch, on by default), spawned by the arc at BreakerOffset in its frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breaker")
	TSubclassOf<AHollowSwitch> BreakerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breaker")
	FTransform BreakerOffset;

	/** Greybox emitters on the walls (engine cubes, instance transforms in the arc's frame): always there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	TArray<FTransform> Emitters;

	/** The arcs themselves (engine cubes): shown only while it strikes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	TArray<FTransform> Beams;

	/** How fast the warning light crackles (on, off) while the telegraph lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "1", Units = "Hertz"))
	float CrackleHz = 0.f;

	/** Sounds of a warning and of a strike (DA_SoundBank, players only; S7-03). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (Categories = "Hollow.Sound"))
	FGameplayTag TelegraphSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (Categories = "Hollow.Sound"))
	FGameplayTag DischargeSound;

	/** The light during the warning, as a fraction of its own intensity (a strike is full). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", ClampMax = "1"))
	float TelegraphGlow = 0.f;

	FHollowArcTimings GetTimings() const
	{
		FHollowArcTimings Timings;
		Timings.TelegraphSeconds = TelegraphSeconds;
		Timings.DischargeSeconds = DischargeSeconds;
		Timings.IdleSeconds = IdleSeconds;
		return Timings;
	}
};
