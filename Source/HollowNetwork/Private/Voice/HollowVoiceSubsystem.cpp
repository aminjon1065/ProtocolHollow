#include "Voice/HollowVoiceSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HollowNetworkLog.h"
#include "Interfaces/VoiceCapture.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "VoiceModule.h"
#include "Voice/HollowVoiceRules.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVoiceSubsystem)

UHollowVoiceSubsystem* UHollowVoiceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UHollowVoiceSubsystem>() : nullptr;
}

void UHollowVoiceSubsystem::SetTransmitting(APlayerController* Player, const bool bTransmit)
{
	// Only a real local player (a ULocalPlayer: a viewport and a controller id) has a microphone; test worlds and
	// headless processes never open one.
	if (bTransmit == bTransmitting || !Player || !Player->GetLocalPlayer())
	{
		return;
	}
	bTransmitting = bTransmit;
	Player->ToggleSpeaking(bTransmit);   // the engine's networked voice through the online subsystem (Steam, Null...)
	if (bTransmit)
	{
		StartMeter();
	}
	else
	{
		StopMeter();
	}
	UE_LOG(LogHollowNet, Verbose, TEXT("Voice: %s %s transmitting"), *Player->GetName(), bTransmit ? TEXT("started") : TEXT("stopped"));
}

bool UHollowVoiceSubsystem::StartMeter()
{
	if (bMeterFailed)
	{
		return false;
	}
	if (!Meter.IsValid() && FVoiceModule::IsAvailable())
	{
		Meter = FVoiceModule::Get().CreateVoiceCapture(FString());   // the default input device
	}
	if (!Meter.IsValid() || !Meter->Start())
	{
		// No microphone, no permission or voice disabled: the voice cannot reach the AI. Said once.
		bMeterFailed = true;
		Meter.Reset();
		UE_LOG(LogHollowNet, Warning, TEXT("Voice: no microphone meter (no input device, no permission, or [Voice] bEnabled=false): the AI will not hear this player's voice."));
		return false;
	}
	return true;
}

void UHollowVoiceSubsystem::StopMeter()
{
	if (Meter.IsValid())
	{
		Meter->Stop();
	}
}

TOptional<float> UHollowVoiceSubsystem::ReadMicAmplitude()
{
	if (!bTransmitting || !Meter.IsValid() || !Meter->IsCapturing())
	{
		return {};
	}
	uint32 Available = 0;
	if (Meter->GetCaptureState(Available) != EVoiceCaptureState::Ok || Available == 0)
	{
		return 0.f;
	}
	MeterBuffer.SetNumUninitialized(Available, EAllowShrinking::No);
	uint32 Read = 0;
	if (Meter->GetVoiceData(MeterBuffer.GetData(), Available, Read) != EVoiceCaptureState::Ok || Read < sizeof(int16))
	{
		return 0.f;
	}
	// The capture gives 16-bit PCM (what the voice encoder takes).
	const TConstArrayView<int16> Samples(reinterpret_cast<const int16*>(MeterBuffer.GetData()), static_cast<int32>(Read / sizeof(int16)));
	return HollowVoiceRules::RmsAmplitude(Samples);
}

FString UHollowVoiceSubsystem::Describe() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const IOnlineSubsystem* Online = World ? Online::GetSubsystem(World) : nullptr;
	const IOnlineVoicePtr Voice = Online ? Online->GetVoiceInterface() : nullptr;
	return FString::Printf(TEXT("backend %s (voice %s), transmitting %d, meter %s"), Online ? *Online->GetSubsystemName().ToString() : TEXT("none"),
		Voice.IsValid() ? TEXT("on") : TEXT("off"), bTransmitting ? 1 : 0,
		bMeterFailed ? TEXT("failed") : (Meter.IsValid() && Meter->IsCapturing() ? TEXT("capturing") : TEXT("idle")));
}

void UHollowVoiceSubsystem::Deinitialize()
{
	if (Meter.IsValid())
	{
		Meter->Shutdown();
		Meter.Reset();
	}
	Super::Deinitialize();
}
