#pragma once

#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"

#include "HollowSoundBank.generated.h"

class UHollowNoiseConfig;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
struct FHollowNoiseRule;

/** One sound of the game by tag (ADR-0022): what plays, how far it carries and how many may play at once. */
USTRUCT(BlueprintType)
struct HOLLOWCORE_API FHollowSoundEntry
{
	GENERATED_BODY()

	/** Hollow.Noise.* (the AI hears it too: same rule, same range) or Hollow.Sound.* (players only, or linked below). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (Categories = "Hollow.Noise,Hollow.Sound"))
	FGameplayTag SoundTag;

	/** Procedural MetaSound (no wave banks). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> Sound;

	/** A Hollow.Sound.* the AI hears as this noise (the mine's blast as Impact.Heavy): its range comes from that rule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (Categories = "Hollow.Noise"))
	FGameplayTag Noise;

	/** Range of a sound no AI hears; a noise's range always comes from DA_NoiseConfig. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0", Units = "Centimeters"))
	float AudibleRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float Volume = 1.f;

	/** Each play is pitched up or down by up to this fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0", ClampMax = "0.5"))
	float PitchVariation = 0.f;

	/** Shape and curve of the falloff; its distance is the range above. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundConcurrency> Concurrency;

	/** The noise rule this sound shares its range with, or none. */
	FGameplayTag GetNoiseTag() const;
};

/** Every gameplay sound by tag (DA_SoundBank, UHollowAudioSettings::SoundBank). */
UCLASS(BlueprintType)
class HOLLOWCORE_API UHollowSoundBank : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The bank named by UHollowAudioSettings (loaded on first use), or null. */
	static const UHollowSoundBank* Get();

	const FHollowSoundEntry* Find(const FGameplayTag& SoundTag) const;

#if WITH_EDITOR
	/** Against the shipped DA_NoiseConfig: HollowSoundRules::Check. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (TitleProperty = "SoundTag"))
	TArray<FHollowSoundEntry> Entries;
};

/** Which sound bank the game uses and whose hearing defines how far players hear noises (DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Audio"))
class HOLLOWCORE_API UHollowAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<UHollowSoundBank> SoundBank;

	/** A noise carries Loudness x this for players (capped by its MaxRange): the reference enemy's hearing (Iron Pilgrim). */
	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "1", Units = "Centimeters"))
	float ReferenceHearingRange = 0.f;

	/** A replicated moment older than this when an actor appears on a machine is old news: not played (late arrivals). */
	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0", Units = "Seconds"))
	float FreshEventSeconds = 0.f;

	/** Whether a moment at ServerTime is still fresh at ServerNow. */
	bool IsFresh(const double ServerTime, const double ServerNow) const { return ServerTime > 0.0 && ServerNow - ServerTime <= FreshEventSeconds; }
};

/** Sound and noise agree (ADR-0022): pure rules over the bank and DA_NoiseConfig. */
namespace HollowSoundRules
{
	/** How far a noise carries for the reference listener: Loudness x ReferenceHearingRange, capped by MaxRange (if any). */
	HOLLOWCORE_API float AudibleRange(const FHollowNoiseRule& Rule, float ReferenceHearingRange);

	/** How far a bank entry carries: its noise rule's range, or its own AudibleRange; 0 if its rule is missing. */
	HOLLOWCORE_API float AudibleRange(const FHollowSoundEntry& Entry, const UHollowNoiseConfig& Noise, float ReferenceHearingRange);

	/**
	 * What breaks the agreement: a noise rule players cannot hear (no entry), an entry without a sound, a duplicate tag,
	 * a link to a noise without a rule, a player-only sound without a range.
	 */
	HOLLOWCORE_API TArray<FString> Check(TConstArrayView<FHollowSoundEntry> Entries, const UHollowNoiseConfig& Noise);
}
