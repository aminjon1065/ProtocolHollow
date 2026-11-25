#pragma once

#include "Engine/DataAsset.h"
#include "FX/HollowFx.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "HollowPulseMineDefinition.generated.h"

class UGameplayEffect;

/** Tuning of the kinetic pulse mine (ADR-0019): when it arms, what springs it, whom it stuns and how loud it is. */
UCLASS(BlueprintType)
class HOLLOWGAME_API UHollowPulseMineDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** After it is placed, before it can go off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine", meta = (ClampMin = "0", Units = "Seconds"))
	float ArmSeconds = 0.f;

	/** How often an armed mine looks for enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine", meta = (ClampMin = "0.02", Units = "Seconds"))
	float CheckSeconds = 0.f;

	/** An enemy (never a player) this close sets it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine", meta = (ClampMin = "0", Units = "Centimeters"))
	float TriggerRadius = 0.f;

	/** Every enemy this close and not behind a wall is stunned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine", meta = (ClampMin = "0", Units = "Centimeters"))
	float BlastRadius = 0.f;

	/** Height above the mine the line of sight to an enemy starts from (off the floor it lies on). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine", meta = (ClampMin = "0", Units = "Centimeters"))
	float SightHeight = 0.f;

	/** The stun (GE_State_Stunned: Hollow.State.Stunned for its duration), applied by the mine's owner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine")
	TSubclassOf<UGameplayEffect> StunEffect;

	/** What the AI hears of the blast (a player's noise: it heats the room, ADR-0018). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine")
	FGameplayTag BlastNoise;

	/** The flash stays this long, then the mine is gone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine", meta = (ClampMin = "0.01", Units = "Seconds"))
	float FlashSeconds = 0.f;

	/** FAB-01 (ADR-0051): the Aether collapsing where it went off, on every machine (none under Reduced Flashes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine")
	FHollowFxSpec BlastFx;

	/** It is laid on the floor found this far below the placer's feet, at the feet otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mine", meta = (ClampMin = "0", Units = "Centimeters"))
	float PlaceTraceDistance = 0.f;
};
