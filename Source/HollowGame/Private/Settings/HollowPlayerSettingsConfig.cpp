#include "Settings/HollowPlayerSettingsConfig.h"

#include "SoundControlBus.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPlayerSettingsConfig)

float FHollowSettingRange::Snap(const float Value) const
{
	const float Low = FMath::Min(Min, Max);
	const float High = FMath::Max(Min, Max);
	const float Clamped = FMath::Clamp(Value, Low, High);
	if (Step <= 0.f)
	{
		return Clamped;
	}
	return FMath::Clamp(Low + FMath::RoundToFloat((Clamped - Low) / Step) * Step, Low, High);
}

float FHollowSettingRange::Stepped(const float Value, const int32 Steps) const
{
	return Snap(Snap(Value) + static_cast<float>(Steps) * Step);
}

const TSoftObjectPtr<USoundControlBus>& UHollowPlayerSettingsConfig::GetBus(const EHollowVolume Which) const
{
	switch (Which)
	{
	case EHollowVolume::Effects:
		return EffectsVolumeBus;
	case EHollowVolume::Voice:
		return VoiceVolumeBus;
	default:
		return MasterVolumeBus;
	}
}
