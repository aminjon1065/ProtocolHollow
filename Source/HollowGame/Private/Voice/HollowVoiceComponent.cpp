#include "Voice/HollowVoiceComponent.h"

#include "Character/HollowExoFrameCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HollowGameLog.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Settings/HollowGameUserSettings.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"
#include "Voice/HollowVoiceSettings.h"
#include "Voice/HollowVoiceSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVoiceComponent)

UHollowVoiceComponent::UHollowVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);   // its server RPC needs a replicated component (the controller reaches only its owner)
}

UHollowVoiceComponent* UHollowVoiceComponent::Get(const AController* Controller)
{
	return Controller ? Controller->FindComponentByClass<UHollowVoiceComponent>() : nullptr;
}

APlayerController* UHollowVoiceComponent::GetPlayer() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UHollowVoiceComponent::IsLocalPlayer() const
{
	const APlayerController* Player = GetPlayer();
	return Player && Player->IsLocalController();
}

void UHollowVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	const UHollowVoiceSettings* Settings = GetDefault<UHollowVoiceSettings>();
	Throttle = Settings->MakeThrottle();
	Guard = Settings->MakeGuard();
	if (IsLocalPlayer())
	{
		const UHollowGameUserSettings* UserSettings = UHollowGameUserSettings::Get();
		Mode = UserSettings ? UserSettings->GetVoiceMode() : EHollowVoiceMode::PushToTalk;
		ApplyTransmission();   // open mic starts at once
	}
}

void UHollowVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalPlayer())
	{
		if (UHollowVoiceSubsystem* Voice = UHollowVoiceSubsystem::Get(this))
		{
			Voice->SetTransmitting(GetPlayer(), false);
		}
	}
	if (UWorld* World = GetWorld())   // gone when a dead world's components end play from garbage collection
	{
		World->GetTimerManager().ClearTimer(SampleTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UHollowVoiceComponent::SetMode(const EHollowVoiceMode NewMode)
{
	Mode = NewMode;
	if (UHollowGameUserSettings* UserSettings = UHollowGameUserSettings::Get())
	{
		UserSettings->SetVoiceMode(NewMode);
	}
	UE_LOG(LogHollowGame, Log, TEXT("Voice: %s mode %s"), *GetNameSafe(GetOwner()), *HollowEnumToString(NewMode));
	ApplyTransmission();
}

void UHollowVoiceComponent::SetPushToTalk(const bool bHeld)
{
	bPushToTalkHeld = bHeld;
	ApplyTransmission();
}

bool UHollowVoiceComponent::IsTransmitting() const
{
	const bool bSimulating = SimulateUntil > 0.0 && GetWorld() && GetWorld()->GetTimeSeconds() < SimulateUntil;
	return bSimulating || Mode == EHollowVoiceMode::OpenMic || (Mode == EHollowVoiceMode::PushToTalk && bPushToTalkHeld);
}

void UHollowVoiceComponent::Simulate(const uint8 InLevel, const float Seconds)
{
	SimulatedLevel = InLevel;
	SimulateUntil = GetWorld()->GetTimeSeconds() + FMath::Max(Seconds, 0.f);
	UE_LOG(LogHollowGame, Log, TEXT("Voice: %s simulates level %d for %.1f s"), *GetNameSafe(GetOwner()), InLevel, Seconds);
	ApplyTransmission();
}

void UHollowVoiceComponent::ApplyTransmission()
{
	APlayerController* Player = GetPlayer();
	UWorld* World = GetWorld();
	if (!Player || !Player->IsLocalController() || !World)
	{
		return;
	}
	// The backend sends what the player chose; a simulation stays local to the report path.
	const bool bBackend = Mode == EHollowVoiceMode::OpenMic || (Mode == EHollowVoiceMode::PushToTalk && bPushToTalkHeld);
	if (UHollowVoiceSubsystem* Voice = UHollowVoiceSubsystem::Get(this))
	{
		Voice->SetTransmitting(Player, bBackend);
	}

	FTimerManager& Timers = World->GetTimerManager();
	const bool bTransmitting = IsTransmitting();
	if (bTransmitting && !Timers.IsTimerActive(SampleTimer))
	{
		Throttle.Reset();
		const float Interval = FMath::Max(GetDefault<UHollowVoiceSettings>()->SampleSeconds, UE_KINDA_SMALL_NUMBER);
		Timers.SetTimer(SampleTimer, this, &ThisClass::Sample, Interval, /*bLoop=*/true);
	}
	else if (!bTransmitting)
	{
		Timers.ClearTimer(SampleTimer);
		SetLevel(0);
	}
	if (bTransmitting != bWasTransmitting)
	{
		bWasTransmitting = bTransmitting;
		OnVoiceActivityChanged.Broadcast(bTransmitting, Level);
	}
}

void UHollowVoiceComponent::Sample()
{
	if (!IsTransmitting())
	{
		ApplyTransmission();   // a simulation ran out
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	uint8 NewLevel = 0;
	if (Now < SimulateUntil)
	{
		NewLevel = SimulatedLevel;
	}
	else if (UHollowVoiceSubsystem* Voice = UHollowVoiceSubsystem::Get(this))
	{
		if (const TOptional<float> Amplitude = Voice->ReadMicAmplitude())
		{
			NewLevel = HollowVoiceRules::LevelFromAmplitude(Amplitude.GetValue(), GetDefault<UHollowVoiceSettings>()->FloorDb);
		}
	}
	SetLevel(NewLevel);
	if (Throttle.ShouldSend(NewLevel, Now))
	{
		Server_ReportVoiceLoudness(NewLevel);
	}
}

void UHollowVoiceComponent::SetLevel(const uint8 NewLevel)
{
	if (NewLevel != Level)
	{
		Level = NewLevel;
		OnVoiceActivityChanged.Broadcast(IsTransmitting(), Level);
	}
}

void UHollowVoiceComponent::Server_ReportVoiceLoudness_Implementation(const uint8 InLevel)
{
	HandleReport(InLevel);
}

bool UHollowVoiceComponent::HandleReport(const uint8 InLevel)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Guard.Admit(Now))
	{
		UE_LOG(LogHollowGame, Verbose, TEXT("Voice: %s reports too fast; dropped."), *GetNameSafe(GetOwner()));
		return false;
	}
	// Only a living body is heard: the dead and spectators talk on the radio, out of the AI's reach.
	const AController* Controller = Cast<AController>(GetOwner());
	AHollowExoFrameCharacter* Body = Controller ? Cast<AHollowExoFrameCharacter>(Controller->GetPawn()) : nullptr;
	const UHollowVoiceSettings* Settings = GetDefault<UHollowVoiceSettings>();
	const FGameplayTag Tier = HollowVoiceRules::TierFor(InLevel, Settings->Tiers);
	if (!Body || Body->IsDead() || !Tier.IsValid())
	{
		return false;
	}
	UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this);
	const bool bHeard = Noise && Noise->ReportNoise(FHollowNoiseEvent(Tier, Body->GetPawnViewLocation(), Body));
	ReportsHeard += bHeard ? 1 : 0;
	const bool bNewUtterance = LastReportTime < 0.0 || Now - LastReportTime > Settings->UtterancePauseSeconds;
	if (bNewUtterance || Tier != LastTier)
	{
		UE_LOG(LogHollowGame, Log, TEXT("Voice: %s %s (level %d)"), *GetNameSafe(Controller), *Tier.ToString(), InLevel);
	}
	UE_LOG(LogHollowGame, Verbose, TEXT("Voice: %s level %d -> %s%s"), *GetNameSafe(Controller), InLevel, *Tier.ToString(), bHeard ? TEXT("") : TEXT(" (rate-limited)"));
	LastTier = Tier;
	LastReportTime = Now;
	return bHeard;
}

FString UHollowVoiceComponent::Describe() const
{
	return FString::Printf(TEXT("mode %s, push-to-talk %d, transmitting %d, level %d, server: %d noise(s), last %s"), *HollowEnumToString(Mode),
		bPushToTalkHeld ? 1 : 0, IsTransmitting() ? 1 : 0, Level, ReportsHeard, LastTier.IsValid() ? *LastTier.ToString() : TEXT("none"));
}
