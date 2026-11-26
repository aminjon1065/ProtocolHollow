#include "Settings/HollowAudioVolumeSubsystem.h"

#include "AudioModulationStatics.h"
#include "Engine/World.h"
#include "HollowGameLog.h"
#include "Misc/App.h"
#include "Settings/HollowGameUserSettings.h"
#include "Settings/HollowPlayerSettingsConfig.h"
#include "SoundControlBus.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAudioVolumeSubsystem)

namespace HollowAudioVolume
{
	constexpr EHollowVolume All[] = {EHollowVolume::Master, EHollowVolume::Effects, EHollowVolume::Voice};
}

bool UHollowAudioVolumeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UHollowAudioVolumeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowAudioVolumeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		AudioHandle = Settings->OnAudioChanged.AddWeakLambda(this, [this]()
		{
			Apply(GetDefault<UHollowPlayerSettingsConfig>()->VolumeFadeSeconds);
		});
	}
	Apply(/*FadeSeconds=*/0.f);
}

void UHollowAudioVolumeSubsystem::Deinitialize()
{
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		Settings->OnAudioChanged.Remove(AudioHandle);
	}
	AudioHandle.Reset();
	Super::Deinitialize();
}

void UHollowAudioVolumeSubsystem::Apply(const float FadeSeconds)
{
	const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	const UHollowPlayerSettingsConfig* Config = GetDefault<UHollowPlayerSettingsConfig>();
	if (!Settings)
	{
		return;
	}
	Applied.Init(-1.f, static_cast<int32>(UE_ARRAY_COUNT(HollowAudioVolume::All)));
	for (const EHollowVolume Volume : HollowAudioVolume::All)
	{
		USoundControlBus* Bus = Config->GetBus(Volume).LoadSynchronous();   // three tiny assets, loaded once
		if (!Bus)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Volume %s: no bus (UHollowPlayerSettingsConfig)."), *UEnum::GetValueAsString(Volume));
			continue;
		}
		const float Value = Settings->GetVolume(Volume);
		UAudioModulationStatics::SetGlobalBusMixValue(GetWorld(), Bus, Value, FadeSeconds);
		Applied[static_cast<int32>(Volume)] = Value;
	}
}

float UHollowAudioVolumeSubsystem::GetAppliedVolume(const EHollowVolume Volume) const
{
	const int32 Index = static_cast<int32>(Volume);
	return Applied.IsValidIndex(Index) ? Applied[Index] : -1.f;
}
