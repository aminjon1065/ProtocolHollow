#include "Carry/HollowCarrierComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Carry/HollowCarrySettings.h"
#include "Carry/HollowCarryableComponent.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "HollowPhysicsLog.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCarrierComponent)

namespace HollowCarrierPrivate
{
	/** Gameplay ability system levels start at 1; carry effects have no levels. */
	constexpr float EffectLevel = 1.f;
}

UHollowCarrierComponent::UHollowCarrierComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;   // events and timers only (CLAUDE.md sec. 1)
	SetIsReplicatedByDefault(true);              // carries the Server_Drop RPC
}

void UHollowCarrierComponent::SetAttachTargets(USceneComponent* FirstPerson, USceneComponent* ThirdPerson)
{
	FirstPersonTarget = FirstPerson;
	ThirdPersonTarget = ThirdPerson;
}

USceneComponent* UHollowCarrierComponent::GetAttachTarget(const bool bFirstPerson) const
{
	return bFirstPerson ? FirstPersonTarget.Get() : ThirdPersonTarget.Get();
}

void UHollowCarrierComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* Pawn = GetPawn(); Pawn && Pawn->HasAuthority())
	{
		Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &ThisClass::HandleControllerChanged);
	}
}

void UHollowCarrierComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PredictionTimer);
	}
	if (UHollowCarryableComponent* Carryable = Carried.Get())
	{
		if (GetOwner()->HasAuthority())
		{
			if (EndPlayReason == EEndPlayReason::Destroyed)
			{
				ServerDrop(/*bForce=*/true);   // died, left the game, despawned: the object stays where the carrier was
			}
		}
		else
		{
			Carryable->HandleCarrierEndPlay(GetPawn());
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool UHollowCarrierComponent::HandleInteractInput(AActor* Focus)
{
	if (IsCarrying())
	{
		Server_Drop();
		return true;
	}
	UHollowCarryableComponent* Carryable = Focus ? Focus->FindComponentByClass<UHollowCarryableComponent>() : nullptr;
	if (!PendingPickup.IsValid() && Carryable && Carryable->CanBeginSolo(GetPawn()))
	{
		// Claim the slowdown at once: the server sees slow moves no later than the request (ADR-0015).
		PendingPickup = Carryable;
		GetWorld()->GetTimerManager().SetTimer(PredictionTimer, this, &ThisClass::HandlePredictionTimeout,
			FMath::Max(GetDefault<UHollowCarrySettings>()->PickupPredictionTimeoutSeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
		RefreshClaim();
		UE_LOG(LogHollowPhysics, Log, TEXT("%s: predicting the pick-up of %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Carryable->GetOwner()));
	}
	return false;
}

void UHollowCarrierComponent::Server_Drop_Implementation()
{
	ServerDrop();
}

bool UHollowCarrierComponent::ServerPickUp(UHollowCarryableComponent* Carryable)
{
	if (!GetOwner()->HasAuthority() || IsCarrying() || !Carryable)
	{
		return false;
	}
	return Carryable->BeginSolo(GetPawn());   // the state change calls NotifyCarryChanged
}

void UHollowCarrierComponent::ServerDrop(const bool bForce)
{
	UHollowCarryableComponent* Carryable = Carried.Get();
	const APawn* Pawn = GetPawn();
	if (!GetOwner()->HasAuthority() || !Carryable || !Pawn || !Carryable->GetDefinition())
	{
		return;
	}
	Carryable->Release(Pawn->GetVelocity() * Carryable->GetDefinition()->InheritVelocityFraction, bForce);
}

void UHollowCarrierComponent::NotifyCarryChanged(UHollowCarryableComponent* Carryable, const bool bCarrying)
{
	if (bCarrying)
	{
		Carried = Carryable;
		if (PendingPickup.IsValid())
		{
			UE_CLOG(PendingPickup.Get() != Carryable, LogHollowPhysics, Log, TEXT("%s: predicted %s, the server gave %s"), *GetNameSafe(GetOwner()),
				*GetNameSafe(PendingPickup->GetOwner()), *GetNameSafe(Carryable->GetOwner()));
			PendingPickup = nullptr;
			GetWorld()->GetTimerManager().ClearTimer(PredictionTimer);
		}
	}
	else if (Carried.Get() == Carryable)
	{
		Carried = nullptr;
	}

	const EHollowCarryClaim Claim = Carried.IsValid() ? HollowCarry::ClaimFor(Carried->GetCarryState().Mode) : EHollowCarryClaim::None;
	if (GetOwner()->HasAuthority())
	{
		if (Carried.IsValid())
		{
			ApplyCarryEffect(*Carried);
		}
		else
		{
			RemoveCarryEffect();
		}
		if (UHollowCharacterMovementComponent* Movement = GetMovement())
		{
			Movement->SetServerCarry(Claim);
		}
	}
	RefreshClaim();
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: carrying %s (%s)"), *GetNameSafe(GetOwner()), Carried.IsValid() ? *GetNameSafe(Carried->GetOwner()) : TEXT("nothing"),
		*HollowEnumToString(Claim));
}

void UHollowCarrierComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	if (!NewController && IsCarrying())
	{
		ServerDrop(/*bForce=*/true);   // unpossessed (player left, spectating): nobody holds the object any more
	}
}

void UHollowCarrierComponent::RefreshClaim()
{
	const APawn* Pawn = GetPawn();
	UHollowCharacterMovementComponent* Movement = GetMovement();
	if (!Pawn || !Movement || !Pawn->IsLocallyControlled())
	{
		return;   // a remote carrier's claim comes with its saved moves
	}
	EHollowCarryClaim Claim = EHollowCarryClaim::None;
	if (Carried.IsValid())
	{
		Claim = HollowCarry::ClaimFor(Carried->GetCarryState().Mode);
	}
	else if (PendingPickup.IsValid())
	{
		Claim = EHollowCarryClaim::Solo;
	}
	Movement->SetClaimedCarry(Claim);
}

void UHollowCarrierComponent::HandlePredictionTimeout()
{
	if (PendingPickup.IsValid())
	{
		UE_LOG(LogHollowPhysics, Log, TEXT("%s: the server never confirmed the pick-up of %s, claim dropped"), *GetNameSafe(GetOwner()),
			*GetNameSafe(PendingPickup->GetOwner()));
	}
	PendingPickup = nullptr;
	RefreshClaim();
}

void UHollowCarrierComponent::ApplyCarryEffect(const UHollowCarryableComponent& Carryable)
{
	if (CarryEffect.IsValid())
	{
		return;
	}
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const UHollowCarryableDefinition* Definition = Carryable.GetDefinition();
	if (!AbilitySystem || !Definition || !Definition->SoloEffect)
	{
		UE_LOG(LogHollowPhysics, Warning, TEXT("%s: no ability system or carry effect; carrying %s without its effect."), *GetNameSafe(GetOwner()),
			*GetNameSafe(Carryable.GetOwner()));
		return;
	}
	CarryEffect = AbilitySystem->ApplyGameplayEffectToSelf(Definition->SoloEffect.GetDefaultObject(), HollowCarrierPrivate::EffectLevel,
		AbilitySystem->MakeEffectContext());
	CarryEffectOwner = AbilitySystem;
}

void UHollowCarrierComponent::RemoveCarryEffect()
{
	if (!CarryEffect.IsValid())
	{
		return;
	}
	if (UAbilitySystemComponent* AbilitySystem = CarryEffectOwner.Get())
	{
		AbilitySystem->RemoveActiveGameplayEffect(CarryEffect);
	}
	CarryEffect.Invalidate();
	CarryEffectOwner = nullptr;
}

APawn* UHollowCarrierComponent::GetPawn() const
{
	return Cast<APawn>(GetOwner());
}

UHollowCharacterMovementComponent* UHollowCarrierComponent::GetMovement() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character ? Cast<UHollowCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}
