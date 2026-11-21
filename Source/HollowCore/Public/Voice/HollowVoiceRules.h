#pragma once

#include "Containers/ArrayView.h"
#include "GameplayTagContainer.h"

#include "HollowVoiceRules.generated.h"

/** How a player's voice goes out (S4-07, ADR-0025): held key, always on, or never. Saved in the player's settings. */
UENUM(BlueprintType)
enum class EHollowVoiceMode : uint8
{
	PushToTalk,
	OpenMic,
	Muted
};

/** One loudness tier of the voice for the AI: from MinLevel up (until the next tier) it is heard as NoiseTag. */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowVoiceTier
{
	GENERATED_BODY()

	/** Hollow.Noise.Voice.*: its rule in DA_NoiseConfig gives loudness, range and rate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice", meta = (Categories = "Hollow.Noise.Voice"))
	FGameplayTag NoiseTag;

	/** Microphone level (0..255, see HollowVoiceRules::LevelFromAmplitude) where this tier begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	uint8 MinLevel = 0;
};

/**
 * Voice -> AI as pure rules (Docs/Architecture/03 sec. 3.8, ADR-0025): the client measures its microphone, sends one
 * byte, and the server picks the tier. No voice backend appears here.
 */
namespace HollowVoiceRules
{
	/** Root mean square of 16-bit PCM, 0..1 (a full-scale square wave is 1). 0 for no samples. */
	HOLLOWCORE_API float RmsAmplitude(TConstArrayView<int16> Samples);

	/** 0..255 on a decibel scale: FloorDb (negative) or quieter is 0, full scale (0 dBFS) is 255. */
	HOLLOWCORE_API uint8 LevelFromAmplitude(float Amplitude, float FloorDb);

	/** The loudest tier Level reaches, or an empty tag (quieter than every tier: silence for the AI). */
	HOLLOWCORE_API FGameplayTag TierFor(uint8 Level, TConstArrayView<FHollowVoiceTier> Tiers);

	/** The level of the quietest tier, or 256 with no tiers (nothing is ever heard). */
	HOLLOWCORE_API int32 QuietestTierLevel(TConstArrayView<FHollowVoiceTier> Tiers);

	/** The mode after Mode in the system menu's cycle: push-to-talk -> open mic -> muted -> push-to-talk. */
	HOLLOWCORE_API EHollowVoiceMode NextMode(EHollowVoiceMode Mode);
}

/**
 * Client side: which measured levels go to the server. Never one quieter than Gate; never two closer than MinSeconds;
 * an unchanged level (within Delta) again only after RepeatSeconds, so a long shout keeps the AI hearing it.
 */
struct HOLLOWCORE_API FHollowVoiceThrottle
{
	float MinSeconds = 0.f;
	float RepeatSeconds = 0.f;
	uint8 Delta = 0;
	int32 Gate = 0;

	/** Whether Level, measured at Now, goes out (and remembers it if so). */
	bool ShouldSend(uint8 Level, double Now);

	/** Forgets what was sent: the next audible level goes out at once. */
	void Reset();

private:
	double LastSent = -1.0;
	uint8 LastLevel = 0;
};

/** Server side: at most PerSecond reports per player on average, Burst at once (a token bucket against floods). */
struct HOLLOWCORE_API FHollowVoiceRateGuard
{
	float PerSecond = 0.f;
	float Burst = 1.f;

	/** Whether a report arriving at Now is let through. */
	bool Admit(double Now);

private:
	double LastTime = -1.0;
	double Tokens = 0.0;
};
