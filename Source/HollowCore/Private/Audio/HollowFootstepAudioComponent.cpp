#include "Audio/HollowFootstepAudioComponent.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagAssetInterface.h"
#include "HollowCoreLog.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFootstepAudioComponent)

UHollowFootstepAudioComponent::UHollowFootstepAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHollowFootstepAudioComponent::IsNoise() const
{
	return SoundTag.MatchesTag(HollowTags::Noise);
}

float UHollowFootstepAudioComponent::GetInterval() const
{
	if (!IsNoise())
	{
		return Cadence.IsValid() ? Cadence.Interval : 0.f;
	}
	const UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this);
	const UHollowNoiseConfig* Config = Noise ? Noise->GetConfig() : nullptr;
	return Config ? Config->FootstepInterval : 0.f;
}

float UHollowFootstepAudioComponent::VolumeAt(const float Speed) const
{
	if (!IsNoise())
	{
		return Cadence.VolumeAt(Speed);
	}
	const UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this);
	const UHollowNoiseConfig* Config = Noise ? Noise->GetConfig() : nullptr;
	return Config ? Config->FootstepLoudnessScale(Speed) : 0.f;   // the AI hears this step as loud
}

void UHollowFootstepAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	const float Interval = GetInterval();
	UE_CLOG(SoundTag.IsValid() && Interval <= 0.f, LogHollowCore, Warning, TEXT("%s: footsteps %s without a cadence never sound."), *GetNameSafe(GetOwner()),
		*SoundTag.ToString());
	if (UHollowAudioSubsystem::Get(this) && SoundTag.IsValid() && Interval > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateWeakLambda(this, [this] { Step(); }), Interval, /*bLoop=*/true);
	}
}

void UHollowFootstepAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())   // null when the garbage collector ends play
	{
		World->GetTimerManager().ClearTimer(StepTimer);
	}
	Super::EndPlay(EndPlayReason);
}

bool UHollowFootstepAudioComponent::Step()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Audio || !Movement || !Movement->IsMovingOnGround() || Character->bIsCrouched || Character->IsHidden())
	{
		return false;
	}
	const IGameplayTagAssetInterface* Tags = Cast<IGameplayTagAssetInterface>(Character);
	if (Tags && Tags->HasMatchingGameplayTag(HollowTags::State_Dead))
	{
		return false;
	}
	const float Scale = VolumeAt(Character->GetVelocity().Size2D());
	if (Scale <= 0.f)
	{
		return false;
	}
	const FVector Feet = Character->GetActorLocation() - FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Audio->Play(SoundTag, Feet, Scale);
	return true;
}
