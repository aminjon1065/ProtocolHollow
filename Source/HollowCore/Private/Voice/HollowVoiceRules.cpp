#include "Voice/HollowVoiceRules.h"

#include "Math/UnrealMathUtility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVoiceRules)

namespace HollowVoiceRulesPrivate
{
	/** Decibels of an amplitude ratio: 20 log10. */
	constexpr float DecibelsPerDecade = 20.f;
	constexpr float FullScale = static_cast<float>(MAX_int16);
	constexpr int32 MaxLevel = MAX_uint8;
}

float HollowVoiceRules::RmsAmplitude(const TConstArrayView<int16> Samples)
{
	if (Samples.IsEmpty())
	{
		return 0.f;
	}
	double SumOfSquares = 0.0;
	for (const int16 Sample : Samples)
	{
		const double Normalized = static_cast<double>(Sample) / HollowVoiceRulesPrivate::FullScale;
		SumOfSquares += Normalized * Normalized;
	}
	return FMath::Min(static_cast<float>(FMath::Sqrt(SumOfSquares / Samples.Num())), 1.f);
}

uint8 HollowVoiceRules::LevelFromAmplitude(const float Amplitude, const float FloorDb)
{
	if (Amplitude <= 0.f || FloorDb >= 0.f)
	{
		return 0;
	}
	const float Decibels = HollowVoiceRulesPrivate::DecibelsPerDecade * FMath::LogX(10.f, FMath::Min(Amplitude, 1.f));
	const float Fraction = FMath::Clamp(1.f - Decibels / FloorDb, 0.f, 1.f);
	return static_cast<uint8>(FMath::RoundToInt(Fraction * HollowVoiceRulesPrivate::MaxLevel));
}

FGameplayTag HollowVoiceRules::TierFor(const uint8 Level, const TConstArrayView<FHollowVoiceTier> Tiers)
{
	const FHollowVoiceTier* Best = nullptr;
	for (const FHollowVoiceTier& Tier : Tiers)
	{
		if (Tier.NoiseTag.IsValid() && Level >= Tier.MinLevel && (!Best || Tier.MinLevel > Best->MinLevel))
		{
			Best = &Tier;
		}
	}
	return Best ? Best->NoiseTag : FGameplayTag();
}

int32 HollowVoiceRules::QuietestTierLevel(const TConstArrayView<FHollowVoiceTier> Tiers)
{
	int32 Quietest = HollowVoiceRulesPrivate::MaxLevel + 1;
	for (const FHollowVoiceTier& Tier : Tiers)
	{
		if (Tier.NoiseTag.IsValid())
		{
			Quietest = FMath::Min<int32>(Quietest, Tier.MinLevel);
		}
	}
	return Quietest;
}

EHollowVoiceMode HollowVoiceRules::NextMode(const EHollowVoiceMode Mode)
{
	switch (Mode)
	{
	case EHollowVoiceMode::PushToTalk: return EHollowVoiceMode::OpenMic;
	case EHollowVoiceMode::OpenMic:    return EHollowVoiceMode::Muted;
	default:                           return EHollowVoiceMode::PushToTalk;
	}
}

bool FHollowVoiceThrottle::ShouldSend(const uint8 Level, const double Now)
{
	if (Level < Gate)
	{
		return false;   // silence is never sent: the AI hears nothing without a report
	}
	if (LastSent >= 0.0)
	{
		const double Since = Now - LastSent;
		const bool bChanged = FMath::Abs(static_cast<int32>(Level) - static_cast<int32>(LastLevel)) >= Delta;
		if (Since < MinSeconds || (!bChanged && Since < RepeatSeconds))
		{
			return false;
		}
	}
	LastSent = Now;
	LastLevel = Level;
	return true;
}

void FHollowVoiceThrottle::Reset()
{
	LastSent = -1.0;
	LastLevel = 0;
}

bool FHollowVoiceRateGuard::Admit(const double Now)
{
	if (PerSecond <= 0.f || Burst < 1.f)
	{
		return false;
	}
	Tokens = LastTime < 0.0 ? Burst : FMath::Min<double>(Burst, Tokens + (Now - LastTime) * PerSecond);
	LastTime = Now;
	if (Tokens < 1.0)
	{
		return false;
	}
	Tokens -= 1.0;
	return true;
}
