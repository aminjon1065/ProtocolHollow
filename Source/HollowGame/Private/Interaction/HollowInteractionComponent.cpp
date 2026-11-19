#include "Interaction/HollowInteractionComponent.h"

#include "Collision/HollowCollisionChannels.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagAssetInterface.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Interaction/HollowInteractionSettings.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowInteractionComponent)

namespace HollowInteractionPrivate
{
#if ENABLE_DRAW_DEBUG
	TAutoConsoleVariable<bool> CVarDebug(TEXT("Hollow.Interaction.Debug"), false,
		TEXT("Draws the local interaction focus probe and its target."), ECVF_Cheat);
#endif

	/** Eyes of the interacting pawn: the same view model on the client (focus) and the server (validation). */
	bool GetEyes(const AActor* Owner, FVector& OutLocation, FRotator& OutRotation)
	{
		const APawn* Pawn = Cast<APawn>(Owner);
		if (!Pawn)
		{
			return false;
		}
		Pawn->GetActorEyesViewPoint(OutLocation, OutRotation);
		return true;
	}
}

UHollowInteractionComponent::UHollowInteractionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;   // timer-driven (CLAUDE.md sec. 1)
	SetIsReplicatedByDefault(true);              // carries the Server_Interact RPC
}

void UHollowInteractionComponent::SetLocalFocusEnabled(const bool bEnabled)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(FocusTimer);
	if (!bEnabled)
	{
		SetFocus(nullptr);
		return;
	}

	const float RateHz = GetDefault<UHollowInteractionSettings>()->FocusRateHz;
	if (RateHz <= 0.f)
	{
		UE_LOG(LogHollowGame, Error, TEXT("HollowInteractionSettings.FocusRateHz must be positive (DefaultGame.ini)."));
		return;
	}
	World->GetTimerManager().SetTimer(FocusTimer, this, &ThisClass::UpdateFocus, 1.f / RateHz, /*bLoop=*/true);
}

void UHollowInteractionComponent::RequestInteract()
{
	if (AActor* Target = FocusedActor.Get())
	{
		Server_Interact(Target);
	}
}

void UHollowInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FocusTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UHollowInteractionComponent::UpdateFocus()
{
	SetFocus(ProbeFocus());
}

AActor* UHollowInteractionComponent::ProbeFocus() const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	FVector EyeLocation;
	FRotator EyeRotation;
	if (!World || !HollowInteractionPrivate::GetEyes(Owner, EyeLocation, EyeRotation))
	{
		return nullptr;
	}

	const UHollowInteractionSettings* Settings = GetDefault<UHollowInteractionSettings>();
	const FVector End = EyeLocation + EyeRotation.Vector() * Settings->InteractionRange;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowInteractionFocus), /*bTraceComplex=*/false, Owner);

	FHitResult Hit;
	AActor* Focus = nullptr;
	if (World->SweepSingleByChannel(Hit, EyeLocation, End, FQuat::Identity, HollowCollision::Interaction,
		FCollisionShape::MakeSphere(Settings->ProbeRadius), Params))
	{
		AActor* HitActor = Hit.GetActor();
		const IHollowInteractable* Interactable = Cast<IHollowInteractable>(HitActor);
		Focus = (Interactable && Interactable->CanInteract(Owner)) ? HitActor : nullptr;
	}

#if ENABLE_DRAW_DEBUG
	if (HollowInteractionPrivate::CVarDebug.GetValueOnGameThread())
	{
		const float Lifetime = 1.f / FMath::Max(GetDefault<UHollowInteractionSettings>()->FocusRateHz, 1.f);
		DrawDebugLine(World, EyeLocation, End, Focus ? FColor::Green : FColor::Red, false, Lifetime);
		if (Focus)
		{
			DrawDebugString(World, Hit.ImpactPoint, Focus->GetName(), nullptr, FColor::Green, Lifetime);
		}
	}
#endif
	return Focus;
}

void UHollowInteractionComponent::SetFocus(AActor* NewFocus)
{
	if (FocusedActor.Get() == NewFocus)
	{
		return;
	}

	FocusedActor = NewFocus;
	const IHollowInteractable* Interactable = Cast<IHollowInteractable>(NewFocus);
	const FHollowInteractionInfo Info = Interactable ? Interactable->GetInteractionInfo(GetOwner()) : FHollowInteractionInfo();
	UE_LOG(LogHollowGame, Verbose, TEXT("%s focus: %s"), *GetNameSafe(GetOwner()), *GetNameSafe(NewFocus));
	OnFocusChanged.Broadcast(NewFocus, Info);
}

FHollowInteractionCheck UHollowInteractionComponent::MeasureRequest(AActor* Target) const
{
	FHollowInteractionCheck Check;
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	Check.bTargetValid = IsValid(Target) && World;
	if (!Check.bTargetValid)
	{
		return Check;
	}

	const IHollowInteractable* Interactable = Cast<IHollowInteractable>(Target);
	Check.bTargetInteractable = Interactable != nullptr;
	if (const IGameplayTagAssetInterface* OwnerTags = Cast<IGameplayTagAssetInterface>(Owner))
	{
		Check.bInteractorBlocked = OwnerTags->HasAnyMatchingGameplayTags(GetDefault<UHollowInteractionSettings>()->BlockingTags);
	}
	if (LastInteractionTime.IsSet())
	{
		Check.SecondsSinceLastInteraction = World->GetTimeSeconds() - LastInteractionTime.GetValue();
	}

	// Line of sight from the eyes to the target's collision bounds: the first blocking hit must be the target itself.
	FVector EyeLocation;
	FRotator EyeRotation;
	if (HollowInteractionPrivate::GetEyes(Owner, EyeLocation, EyeRotation))
	{
		FVector Origin;
		FVector Extent;
		Target->GetActorBounds(/*bOnlyCollidingComponents=*/true, Origin, Extent);
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowInteractionValidate), /*bTraceComplex=*/false, Owner);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, EyeLocation, Origin, HollowCollision::Interaction, Params) && Hit.GetActor() == Target)
		{
			Check.bHasLineOfSight = true;
			Check.Distance = Hit.Distance;
		}
	}

	Check.bTargetAllows = Interactable && Interactable->CanInteract(Owner);
	return Check;
}

void UHollowInteractionComponent::Server_Interact_Implementation(AActor* Target)
{
	const UHollowInteractionSettings* Settings = GetDefault<UHollowInteractionSettings>();
	const EHollowInteractionRejection Rejection = HollowInteractionRules::ValidateRequest(MeasureRequest(Target),
		Settings->InteractionRange + Settings->ServerRangeTolerance, Settings->MinInteractInterval);
	if (Rejection != EHollowInteractionRejection::None)
	{
		// Soft rejection (no kick): with latency a fair request can land just outside the rules.
		UE_LOG(LogHollowGame, Verbose, TEXT("%s: interaction with %s rejected (%s)."), *GetNameSafe(GetOwner()), *GetNameSafe(Target),
			*HollowEnumToString(Rejection));
		return;
	}

	LastInteractionTime = GetWorld()->GetTimeSeconds();
	Cast<IHollowInteractable>(Target)->Interact(GetOwner());
}
