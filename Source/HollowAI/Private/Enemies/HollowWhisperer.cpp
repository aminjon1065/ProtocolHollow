#include "Enemies/HollowWhisperer.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Brain/HollowWhispererController.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Enemies/HollowWhispererDefinition.h"
#include "HollowAILog.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWhisperer)

AHollowWhisperer::AHollowWhisperer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AHollowWhispererController::StaticClass();
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);   // looks only: the capsule is what is hit
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
}

void AHollowWhisperer::BeginPlay()
{
	Super::BeginPlay();
	if (AbilitySystem)
	{
		LitHandle = AbilitySystem->RegisterGameplayTagEvent(HollowTags::State_Lit, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleLitChanged);
		FrenzyHandle = AbilitySystem->RegisterGameplayTagEvent(HollowTags::State_Frenzy, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleFrenzyChanged);
	}
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	if (Audio && GetNetMode() != NM_DedicatedServer && !IsDead())
	{
		Whisper = Audio->PlayAttached(HollowTags::Sound_Whisperer_Whisper, GetRootComponent());
		WhisperVolume = Whisper ? Whisper->VolumeMultiplier : 1.f;
	}
}

void AHollowWhisperer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystem)
	{
		AbilitySystem->UnregisterGameplayTagEvent(LitHandle, HollowTags::State_Lit, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(FrenzyHandle, HollowTags::State_Frenzy, EGameplayTagEventType::NewOrRemoved);
	}
	StopWhisper();
	Super::EndPlay(EndPlayReason);
}

void AHollowWhisperer::HandleLitChanged(const FGameplayTag Tag, const int32 Count)
{
	if (IsDead())
	{
		return;
	}
	Body->SetCustomPrimitiveDataFloat(FlareDataIndex, Count > 0 ? 1.f : 0.f);
	if (Count > 0)
	{
		++Shrieks;
		if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this); Audio && GetNetMode() != NM_DedicatedServer)
		{
			Audio->Play(HollowTags::Sound_Whisperer_Shriek, GetActorLocation());
		}
	}
}

void AHollowWhisperer::HandleFrenzyChanged(const FGameplayTag Tag, const int32 Count)
{
	const UHollowWhispererDefinition* Whisperer = Cast<UHollowWhispererDefinition>(GetDefinition());
	if (Whisper && Whisperer)
	{
		Whisper->SetVolumeMultiplier(WhisperVolume * (Count > 0 ? Whisperer->FrenzyWhisperVolume : 1.f));
	}
}

void AHollowWhisperer::StopWhisper()
{
	if (Whisper)
	{
		Whisper->Stop();
		Whisper = nullptr;
	}
}

void AHollowWhisperer::PresentDeath()
{
	StopWhisper();
	Body->SetCustomPrimitiveDataFloat(FlareDataIndex, 0.f);
	Body->SetCustomPrimitiveDataFloat(DeadDataIndex, 1.f);
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this); Audio && GetNetMode() != NM_DedicatedServer)
	{
		Audio->Play(HollowTags::Sound_Whisperer_Death, GetActorLocation());
	}
	UE_LOG(LogHollowAI, Verbose, TEXT("%s: dead on this machine"), *GetName());
}
