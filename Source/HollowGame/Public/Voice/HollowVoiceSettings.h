#pragma once

#include "Engine/DeveloperSettings.h"
#include "Voice/HollowVoiceRules.h"

#include "HollowVoiceSettings.generated.h"

class UHollowVoiceConfig;

/**
 * Voice -> AI tuning (S4-07, ADR-0025; DefaultGame.ini): the microphone scale, how often a client reports, what the
 * server accepts, and the loudness tiers. How loud each tier is for the AI is its rule in DA_NoiseConfig.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Voice"))
class HOLLOWGAME_API UHollowVoiceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** DA_VoiceConfig: the push-to-talk input, the talker's attenuation and its helmet and radio effects. */
	UPROPERTY(Config, EditAnywhere, Category = "Voice")
	TSoftObjectPtr<UHollowVoiceConfig> VoiceConfig;

	/** Microphone level 0 is this loud (dBFS) or quieter; 255 is full scale. */
	UPROPERTY(Config, EditAnywhere, Category = "Microphone", meta = (ClampMax = "-1"))
	float FloorDb = 0.f;

	/** While transmitting, the microphone is measured this often (a timer, no tick). */
	UPROPERTY(Config, EditAnywhere, Category = "Microphone", meta = (ClampMin = "0.02", Units = "Seconds"))
	float SampleSeconds = 0.f;

	/** A client reports its level at most this often (one byte, unreliable, Docs/Architecture/03 sec. 3.1). */
	UPROPERTY(Config, EditAnywhere, Category = "Reports", meta = (ClampMin = "0.1"))
	float MaxReportsPerSecond = 0.f;

	/** A steady level is reported again this often, so a long shout stays heard. */
	UPROPERTY(Config, EditAnywhere, Category = "Reports", meta = (ClampMin = "0", Units = "Seconds"))
	float RepeatSeconds = 0.f;

	/** A level change this big is reported sooner (within the rate). */
	UPROPERTY(Config, EditAnywhere, Category = "Reports")
	uint8 ReportDelta = 0;

	/** The server takes this many reports of one player at once (network jitter), MaxReportsPerSecond on average. */
	UPROPERTY(Config, EditAnywhere, Category = "Reports", meta = (ClampMin = "1"))
	float ServerBurst = 1.f;

	/** A pause this long ends what the server logs as one utterance. */
	UPROPERTY(Config, EditAnywhere, Category = "Reports", meta = (ClampMin = "0", Units = "Seconds"))
	float UtterancePauseSeconds = 0.f;

	/** Whisper, speech, shout: the level where each begins (quieter than the first: the AI hears nothing). */
	UPROPERTY(Config, EditAnywhere, Category = "Tiers", meta = (TitleProperty = "NoiseTag"))
	TArray<FHollowVoiceTier> Tiers;

	/** DA_VoiceConfig, loaded on first use (small; every machine needs it). */
	const UHollowVoiceConfig* LoadConfig() const;

	FHollowVoiceThrottle MakeThrottle() const;
	FHollowVoiceRateGuard MakeGuard() const;
};
