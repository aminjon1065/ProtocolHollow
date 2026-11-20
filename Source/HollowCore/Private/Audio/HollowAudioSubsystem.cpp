#include "Audio/HollowAudioSubsystem.h"

#include "Audio/HollowSoundBank.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "HollowCoreLog.h"
#include "Kismet/GameplayStatics.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSettings.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAudioSubsystem)

UHollowAudioSubsystem* UHollowAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowAudioSubsystem>() : nullptr;
}

bool UHollowAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);   // nobody listens to a dedicated server
}

bool UHollowAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SetSources(UHollowSoundBank::Get(), GetDefault<UHollowNoiseSettings>()->Config.LoadSynchronous());
}

void UHollowAudioSubsystem::SetSources(const UHollowSoundBank* InBank, const UHollowNoiseConfig* InNoise)
{
	Bank = InBank;
	NoiseConfig = InNoise;
	Resolve();
}

void UHollowAudioSubsystem::Resolve()
{
	Resolved.Reset();
	Keep.Reset();
	if (!Bank || !NoiseConfig)
	{
		return;
	}
	const float Reference = GetDefault<UHollowAudioSettings>()->ReferenceHearingRange;
	for (const FHollowSoundEntry& Entry : Bank->Entries)
	{
		FResolved& Out = Resolved.Add(Entry.SoundTag);
		Out.Sound = Entry.Sound.LoadSynchronous();
		Out.Concurrency = Entry.Concurrency.LoadSynchronous();
		Out.Volume = Entry.Volume;
		Out.PitchVariation = Entry.PitchVariation;
		Out.Range = HollowSoundRules::AudibleRange(Entry, *NoiseConfig, Reference);
		// The template's shape and curve, the tag's range: inner radius, then falloff to silence at Range.
		const USoundAttenuation* Template = Entry.Attenuation.LoadSynchronous();
		USoundAttenuation* Attenuation = Template ? DuplicateObject<USoundAttenuation>(Template, this) : NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents.X = FMath::Min(Settings.AttenuationShapeExtents.X, Out.Range * 0.5f);
		Settings.FalloffDistance = FMath::Max(Out.Range - Settings.AttenuationShapeExtents.X, 1.f);
		Out.Attenuation = Attenuation;
		for (UObject* Object : {static_cast<UObject*>(Attenuation), static_cast<UObject*>(Out.Sound), static_cast<UObject*>(Out.Concurrency)})
		{
			if (Object)
			{
				Keep.Add(Object);
			}
		}
	}
}

const UHollowAudioSubsystem::FResolved* UHollowAudioSubsystem::Count(const FGameplayTag& SoundTag)
{
	++Counts.FindOrAdd(SoundTag);
	const FResolved* Entry = Resolved.Find(SoundTag);
	UE_CLOG(!Entry, LogHollowCore, Warning, TEXT("No sound for %s (DA_SoundBank)."), *SoundTag.ToString());
	UE_LOG(LogHollowCore, Verbose, TEXT("Sound %s"), *SoundTag.ToString());
	return Entry && Entry->Sound ? Entry : nullptr;
}

float UHollowAudioSubsystem::RandomPitch(const FResolved& Entry) const
{
	return Entry.PitchVariation > 0.f ? FMath::FRandRange(1.f - Entry.PitchVariation, 1.f + Entry.PitchVariation) : 1.f;   // cosmetic, local
}

UAudioComponent* UHollowAudioSubsystem::Play(const FGameplayTag& SoundTag, const FVector& Location, const float VolumeScale)
{
	const FResolved* Entry = Count(SoundTag);
	return Entry ? UGameplayStatics::SpawnSoundAtLocation(this, Entry->Sound, Location, FRotator::ZeroRotator, Entry->Volume * VolumeScale,
		RandomPitch(*Entry), 0.f, Entry->Attenuation, Entry->Concurrency, /*bAutoDestroy=*/true) : nullptr;
}

UAudioComponent* UHollowAudioSubsystem::PlayAttached(const FGameplayTag& SoundTag, USceneComponent* Parent, const float VolumeScale)
{
	const FResolved* Entry = Parent ? Count(SoundTag) : nullptr;
	return Entry ? UGameplayStatics::SpawnSoundAttached(Entry->Sound, Parent, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
		/*bStopWhenAttachedToDestroyed=*/true, Entry->Volume * VolumeScale, RandomPitch(*Entry), 0.f, Entry->Attenuation, Entry->Concurrency,
		/*bAutoDestroy=*/true) : nullptr;
}

float UHollowAudioSubsystem::GetAudibleRange(const FGameplayTag& SoundTag) const
{
	const FResolved* Entry = Resolved.Find(SoundTag);
	return Entry ? Entry->Range : 0.f;
}

int32 UHollowAudioSubsystem::GetPlayCount(const FGameplayTag& SoundTag) const
{
	const int32* Value = Counts.Find(SoundTag);
	return Value ? *Value : 0;
}

FString UHollowAudioSubsystem::Describe() const
{
	TArray<FGameplayTag> Tags;
	Counts.GetKeys(Tags);
	Tags.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.ToString() < B.ToString(); });
	FString Text = FString::Printf(TEXT("%d sounds in the bank;"), Resolved.Num());
	for (const FGameplayTag& Tag : Tags)
	{
		Text += FString::Printf(TEXT(" %s x%d"), *Tag.ToString(), Counts[Tag]);
	}
	return Text;
}
