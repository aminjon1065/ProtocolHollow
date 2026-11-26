#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "HollowAudioVolumeSubsystem.generated.h"

enum class EHollowVolume : uint8;

/**
 * UI-02 (ADR-0052): the player's volumes on this machine. Each is an Audio Modulation bus (UHollowPlayerSettingsConfig)
 * the sound classes' volume listens to: SC_Hollow_World and SC_Hollow_UI the master and effects buses, SC_Hollow_Voice
 * the master and voice buses. Set when the world begins play and whenever the settings change; no sound is rebuilt.
 */
UCLASS()
class HOLLOWGAME_API UHollowAudioVolumeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Sets every bus to the player's volume (fading over FadeSeconds). */
	void Apply(float FadeSeconds);

	/** What it set a bus to last, or below 0 before it did (tests). */
	float GetAppliedVolume(EHollowVolume Volume) const;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	FDelegateHandle AudioHandle;
	TArray<float> Applied;
};
