#include "Noise/HollowNoiseSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HollowCoreLog.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSettings.h"
#include "Perception/AISense_Hearing.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowNoiseSubsystem)

UHollowNoiseSubsystem* UHollowNoiseSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowNoiseSubsystem>() : nullptr;
}

bool UHollowNoiseSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowNoiseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// A small data asset: loaded with the world, so the first noise never waits.
	Config = GetDefault<UHollowNoiseSettings>()->Config.LoadSynchronous();
	UE_CLOG(!Config, LogHollowCore, Warning, TEXT("No noise config (UHollowNoiseSettings::Config): the AI hears nothing in %s."), *GetNameSafe(GetWorld()));
}

void UHollowNoiseSubsystem::SetConfig(const UHollowNoiseConfig* InConfig)
{
	Config = InConfig;
	LastReported.Reset();
	LastAnswered.Reset();
	LastAlerted.Reset();
}

bool UHollowNoiseSubsystem::AnswerNoise(AActor& Enemy, AActor* Instigator, const FGameplayTag& NoiseTag)
{
	const UWorld* World = GetWorld();
	const FHollowNoiseRule* Rule = Config ? Config->FindRule(NoiseTag) : nullptr;
	if (!World || World->GetNetMode() == NM_Client || !Rule || !Rule->bRaisesAcousticAlert)
	{
		return false;
	}
	const double Now = World->GetTimeSeconds();
	const auto IsDue = [Now](TMap<FObjectKey, double>& Last, const UObject* Key, const float Interval)
	{
		double& When = Last.FindOrAdd(FObjectKey(Key), -TNumericLimits<double>::Max());
		if (Now - When < Interval)
		{
			return false;
		}
		When = Now;
		return true;
	};
	if (!IsDue(LastAnswered, &Enemy, Config->AnswerInterval))
	{
		return false;
	}
	UE_LOG(LogHollowCore, Log, TEXT("%s answers %s by %s"), *Enemy.GetName(), *NoiseTag.ToString(), *GetNameSafe(Instigator));
	if (Instigator && IsDue(LastAlerted, Instigator, Config->AcousticAlertCooldown))
	{
		FHollowNoiseAnswer Answer;
		Answer.Enemy = &Enemy;
		Answer.Instigator = Instigator;
		Answer.NoiseTag = NoiseTag;
		OnAcousticAlert.Broadcast(Answer);
	}
	return true;
}

bool UHollowNoiseSubsystem::ReportNoise(const FHollowNoiseEvent& Event)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Config || !Event.NoiseTag.IsValid())
	{
		return false;
	}
	const FHollowNoiseRule* Rule = Config->FindRule(Event.NoiseTag);
	if (!Rule)
	{
		if (!UnknownTags.Contains(Event.NoiseTag))
		{
			UnknownTags.Add(Event.NoiseTag);
			UE_LOG(LogHollowCore, Warning, TEXT("Noise %s has no rule in %s: not reported."), *Event.NoiseTag.ToString(), *GetNameSafe(Config));
		}
		return false;
	}
	const float Loudness = Rule->Loudness * FMath::Max(Event.LoudnessScale, 0.f);
	if (Loudness <= 0.f)
	{
		return false;
	}
	if (Rule->MinInterval > 0.f)
	{
		const double Now = World->GetTimeSeconds();
		double& Last = LastReported.FindOrAdd(TPair<FObjectKey, FGameplayTag>(FObjectKey(Event.Instigator.Get()), Event.NoiseTag), -TNumericLimits<double>::Max());
		if (Now - Last < Rule->MinInterval)
		{
			return false;
		}
		Last = Now;
	}

	UAISense_Hearing::ReportNoiseEvent(World, Event.Location, Loudness, Event.Instigator.Get(), Rule->MaxRange, Event.NoiseTag.GetTagName());
	UE_LOG(LogHollowCore, Verbose, TEXT("Noise %s by %s at %s: loudness %.2f, range %.0f"), *Event.NoiseTag.ToString(), *GetNameSafe(Event.Instigator.Get()),
		*Event.Location.ToCompactString(), Loudness, Rule->MaxRange);
	FHollowNoiseReport Report;
	Report.Event = Event;
	Report.Loudness = Loudness;
	Report.MaxRange = Rule->MaxRange;
	OnNoiseReported.Broadcast(Report);
	return true;
}
