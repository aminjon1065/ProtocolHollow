#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "HollowVoiceSubsystem.generated.h"

class APlayerController;
class IVoiceCapture;

/**
 * The only place that knows the voice backend (S4-07, ADR-0025): it starts and stops sending this machine's voice
 * (the engine's networked voice: Steam today, EOS or Vivox later without touching gameplay) and measures the local
 * microphone for the AI with its own capture (the engine's Mac capture reports no amplitude, so this reads PCM).
 * Runs on every machine that has a local player; a dedicated server never transmits.
 */
UCLASS()
class HOLLOWNETWORK_API UHollowVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UHollowVoiceSubsystem* Get(const UObject* WorldContext);

	/** Starts or stops sending Player's voice and the microphone meter with it (a local player with a ULocalPlayer only). */
	void SetTransmitting(APlayerController* Player, bool bTransmit);

	bool IsTransmitting() const { return bTransmitting; }

	/**
	 * Loudness of what the microphone captured since the last read (RMS 0..1), draining it; unset while not
	 * transmitting or without a capture device.
	 */
	TOptional<float> ReadMicAmplitude();

	/** One line about the backend and the meter (Hollow.Voice.Dump). */
	FString Describe() const;

protected:
	virtual void Deinitialize() override;

private:
	bool StartMeter();
	void StopMeter();

	TSharedPtr<IVoiceCapture> Meter;
	TArray<uint8> MeterBuffer;
	bool bTransmitting = false;
	bool bMeterFailed = false;
};
