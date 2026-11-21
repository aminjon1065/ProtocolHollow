#pragma once

#include "Engine/DataAsset.h"

#include "HollowVoiceConfig.generated.h"

class UInputAction;
class UInputMappingContext;
class USoundAttenuation;
class USoundEffectSourcePresetChain;

/**
 * The voice's content (DA_VoiceConfig, create_voice_assets.py; S4-07, ADR-0025): the push-to-talk key, and how a
 * player's voice sounds - from the helmet of a living ExoFrame (3D, room reverb, muffled through walls by the rooms'
 * acoustics) or on the radio without a living body (2D).
 */
UCLASS(BlueprintType)
class HOLLOWGAME_API UHollowVoiceConfig : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Held: transmits. Bound by the player controller, so it works alive, dead and in the hub. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> PushToTalkAction;

	/** The voice keys (V, LB), added by every local player controller. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 MappingPriority = 0;

	/** A living player's voice from the helmet: how it fades with distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Talker")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** A living player's voice: the helmet's filter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Talker")
	TObjectPtr<USoundEffectSourcePresetChain> HelmetChain;

	/** A voice without a living body (dead, spectating, the front end): the radio's filter, not spatialized. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Talker")
	TObjectPtr<USoundEffectSourcePresetChain> RadioChain;
};
