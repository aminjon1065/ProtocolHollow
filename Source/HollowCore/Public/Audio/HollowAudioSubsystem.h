#pragma once

#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"

#include "HollowAudioSubsystem.generated.h"

class UAudioComponent;
class UHollowNoiseConfig;
class UHollowSoundBank;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * The one way gameplay sounds play (ADR-0022), on every machine but a dedicated server. Sounds are local and cosmetic:
 * callers play them from replicated state (door and breach states, carry state, cues), never from the server's noise
 * events. Each tag resolves through DA_SoundBank; a noise's range comes from DA_NoiseConfig, so a player hears it as
 * far as the reference enemy does. Counts every request (with or without an audio device) for tests and dumps.
 */
UCLASS()
class HOLLOWCORE_API UHollowAudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowAudioSubsystem* Get(const UObject* WorldContext);

	/** A one-shot at Location; null without the tag, a sound or an audio device. */
	UAudioComponent* Play(const FGameplayTag& SoundTag, const FVector& Location, float VolumeScale = 1.f);

	/** A sound following Parent (loops: the caller stops it, or it ends with Parent). */
	UAudioComponent* PlayAttached(const FGameplayTag& SoundTag, USceneComponent* Parent, float VolumeScale = 1.f);

	/** How far the tag carries (0 if unknown). */
	float GetAudibleRange(const FGameplayTag& SoundTag) const;
	int32 GetPlayCount(const FGameplayTag& SoundTag) const;
	void ResetCounts() { Counts.Reset(); }

	/** Bank and noise rules in use (the shipped ones by default); tests swap in their own. */
	void SetSources(const UHollowSoundBank* InBank, const UHollowNoiseConfig* InNoise);

	FString Describe() const;

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	struct FResolved
	{
		USoundBase* Sound = nullptr;
		USoundAttenuation* Attenuation = nullptr;
		USoundConcurrency* Concurrency = nullptr;
		float Volume = 1.f;
		float PitchVariation = 0.f;
		float Range = 0.f;
	};

	void Resolve();
	const FResolved* Count(const FGameplayTag& SoundTag);
	float RandomPitch(const FResolved& Entry) const;

	UPROPERTY(Transient)
	TObjectPtr<const UHollowSoundBank> Bank;

	UPROPERTY(Transient)
	TObjectPtr<const UHollowNoiseConfig> NoiseConfig;

	/** Loaded sounds and the per-tag attenuation copies (their falloff is the tag's range). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Keep;

	TMap<FGameplayTag, FResolved> Resolved;
	TMap<FGameplayTag, int32> Counts;
};
