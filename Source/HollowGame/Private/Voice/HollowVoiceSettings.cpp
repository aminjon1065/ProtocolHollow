#include "Voice/HollowVoiceSettings.h"

#include "Voice/HollowVoiceConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVoiceSettings)

const UHollowVoiceConfig* UHollowVoiceSettings::LoadConfig() const
{
	return VoiceConfig.LoadSynchronous();
}

FHollowVoiceThrottle UHollowVoiceSettings::MakeThrottle() const
{
	FHollowVoiceThrottle Throttle;
	Throttle.MinSeconds = MaxReportsPerSecond > 0.f ? 1.f / MaxReportsPerSecond : 0.f;
	Throttle.RepeatSeconds = RepeatSeconds;
	Throttle.Delta = ReportDelta;
	Throttle.Gate = HollowVoiceRules::QuietestTierLevel(Tiers);
	return Throttle;
}

FHollowVoiceRateGuard UHollowVoiceSettings::MakeGuard() const
{
	FHollowVoiceRateGuard Guard;
	Guard.PerSecond = MaxReportsPerSecond;
	Guard.Burst = ServerBurst;
	return Guard;
}
