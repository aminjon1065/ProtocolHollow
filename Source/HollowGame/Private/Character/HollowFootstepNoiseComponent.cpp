#include "Character/HollowFootstepNoiseComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagAssetInterface.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFootstepNoiseComponent)

UHollowFootstepNoiseComponent::UHollowFootstepNoiseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHollowFootstepNoiseComponent::BeginPlay()
{
	Super::BeginPlay();
	const UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this);
	const UHollowNoiseConfig* Config = Noise ? Noise->GetConfig() : nullptr;
	if (GetOwner()->HasAuthority() && Config && Config->FootstepInterval > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(StepTimer, this, &ThisClass::HandleStepTimer, Config->FootstepInterval, /*bLoop=*/true);
	}
}

void UHollowFootstepNoiseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Also reached from garbage collection of a torn-down world (BeginDestroy ends play): the world may be gone.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UHollowFootstepNoiseComponent::HandleStepTimer()
{
	Step();
}

bool UHollowFootstepNoiseComponent::Step()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this);
	const UHollowNoiseConfig* Config = Noise ? Noise->GetConfig() : nullptr;
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Config || !Movement || !Character->GetController() || !Movement->IsMovingOnGround() || Movement->IsCrouching())
	{
		return false;
	}
	const IGameplayTagAssetInterface* Tags = Cast<IGameplayTagAssetInterface>(Character);
	if (Tags && Tags->HasMatchingGameplayTag(HollowTags::State_Dead))
	{
		return false;
	}
	const float Scale = Config->FootstepLoudnessScale(Character->GetVelocity().Size2D());
	if (Scale <= 0.f)
	{
		return false;
	}
	const FVector Feet = Character->GetActorLocation() - FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Footstep, Feet, Character, Scale));
}
