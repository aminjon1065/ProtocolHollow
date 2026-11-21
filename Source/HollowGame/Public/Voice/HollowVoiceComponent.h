#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Voice/HollowVoiceRules.h"

#include "HollowVoiceComponent.generated.h"

class AController;
class APlayerController;

/** The local player's voice went out or stopped, or its level changed (the visor's microphone indicator, S4-08). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowVoiceActivityChanged, bool /*bTransmitting*/, uint8 /*Level*/);
/** The local player's voice mode changed (push-to-talk, open mic, muted; the visor shows it, S4-08). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowVoiceModeChanged, EHollowVoiceMode /*Mode*/);

/**
 * A player's voice for the AI (S4-07, ADR-0025, Docs/Architecture/03 sec. 3.8), on the player controller (so the dead
 * and spectators keep their push-to-talk).
 *  - Owning client: the mode (push-to-talk, open mic, muted; the player's settings), the backend's transmission
 *    (UHollowVoiceSubsystem), and while transmitting a timer that measures the microphone and reports one byte.
 *  - Server: a living ExoFrame's report becomes the noise of its tier (Hollow.Noise.Voice.*) at its head.
 * The AI hears only a transmitted voice; the voice backend never appears here.
 */
UCLASS(ClassGroup = Hollow)
class HOLLOWGAME_API UHollowVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHollowVoiceComponent();

	static UHollowVoiceComponent* Get(const AController* Controller);

	EHollowVoiceMode GetMode() const { return Mode; }

	/** Owning client: switches the mode now (open mic starts, mute stops) and keeps it in the player's settings. */
	void SetMode(EHollowVoiceMode NewMode);

	/** Owning client: push-to-talk held or released (counts in PushToTalk mode only). */
	void SetPushToTalk(bool bHeld);

	/** Whether this player's voice goes out (and the AI may hear it): push-to-talk held, open mic, or a simulation. */
	bool IsTransmitting() const;

	/** The last measured microphone level (0..255; 0 when not transmitting). */
	uint8 GetLevel() const { return Level; }

	/** Dev and tests: for Seconds the microphone reads InLevel; nothing else changes (no backend, same report path). */
	void Simulate(uint8 InLevel, float Seconds);

	FOnHollowVoiceActivityChanged OnVoiceActivityChanged;
	FOnHollowVoiceModeChanged OnVoiceModeChanged;

	/** The owning client's measured level (the only data of the voice -> AI path: unreliable, at most a few per second). */
	UFUNCTION(Server, Unreliable)
	void Server_ReportVoiceLoudness(uint8 InLevel);

	/** Server: one report, as the RPC handles it; true if a noise went out (rate, living body, audible tier). */
	bool HandleReport(uint8 InLevel);

	/** One line for Hollow.Voice.Dump. */
	FString Describe() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPlayer() const;
	bool IsLocalPlayer() const;
	/** Starts or stops the backend and the measuring timer to match the mode, the key and a simulation. */
	void ApplyTransmission();
	void Sample();
	void SetLevel(uint8 NewLevel);

	EHollowVoiceMode Mode = EHollowVoiceMode::PushToTalk;
	bool bPushToTalkHeld = false;
	bool bWasTransmitting = false;
	uint8 Level = 0;
	uint8 SimulatedLevel = 0;
	double SimulateUntil = -1.0;
	FHollowVoiceThrottle Throttle;
	FTimerHandle SampleTimer;

	// Server
	FHollowVoiceRateGuard Guard;
	FGameplayTag LastTier;
	double LastReportTime = -1.0;
	int32 ReportsHeard = 0;
};
