#include "Feel/HollowFeelCameraShake.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFeelCameraShake)

void UHollowFeelShakePattern::Setup(const FHollowShakeParams& InShake, const uint32 InSeed)
{
	Shake = InShake;
	Seed = InSeed;
	Elapsed = 0.f;
	bStopped = false;
}

void UHollowFeelShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Shake.Duration);
	OutInfo.BlendIn = 0.f;    // the envelope is the pattern's own (HollowFeelRules::Envelope)
	OutInfo.BlendOut = 0.f;
}

void UHollowFeelShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
	bStopped = false;
}

void UHollowFeelShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	Evaluate(OutResult);
}

void UHollowFeelShakePattern::ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed = Params.AbsoluteTime;
	Evaluate(OutResult);
}

bool UHollowFeelShakePattern::IsFinishedImpl() const
{
	return bStopped || Elapsed >= Shake.Duration;
}

void UHollowFeelShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	bStopped = true;
}

void UHollowFeelShakePattern::Evaluate(FCameraShakePatternUpdateResult& OutResult) const
{
	HollowFeelRules::Sample(Shake, Seed, Elapsed, OutResult.Rotation, OutResult.Location);   // the base class scales it and orients it
}

UHollowFeelCameraShake::UHollowFeelCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = false;   // steps and blasts overlap
	SetRootShakePattern(ObjectInitializer.CreateDefaultSubobject<UHollowFeelShakePattern>(this, TEXT("Pattern")));
}

void UHollowFeelCameraShake::Setup(const FHollowShakeParams& Shake, const uint32 Seed)
{
	if (UHollowFeelShakePattern* Pattern = Cast<UHollowFeelShakePattern>(GetRootShakePattern()))
	{
		Pattern->Setup(Shake, Seed);
	}
}

const UHollowFeelShakePattern* UHollowFeelCameraShake::GetPattern() const
{
	return Cast<UHollowFeelShakePattern>(GetRootShakePattern());
}
