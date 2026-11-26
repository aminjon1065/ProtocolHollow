#include "Interaction/HollowInteractionComponent.h"

#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Collision/HollowCollisionChannels.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagAssetInterface.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Interaction/HollowInteractionSettings.h"
#include "Interfaces/HollowCarryReceiver.h"
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

	/** The owner and what it carries (a core in front of the eyes must not hide the altar it is taken to, S7-02). */
	FCollisionQueryParams QueryParams(const AActor* Owner, const FName Stat)
	{
		FCollisionQueryParams Params(Stat, /*bTraceComplex=*/false, Owner);
		const UHollowCarrierComponent* Carrier = Owner ? Owner->FindComponentByClass<UHollowCarrierComponent>() : nullptr;
		if (const UHollowCarryableComponent* Carried = Carrier ? Carrier->GetCarried() : nullptr)
		{
			Params.AddIgnoredActor(Carried->GetOwner());
		}
		return Params;
	}

	/**
	 * Whether the interacting character's state forbids interaction (UHollowInteractionSettings::BlockingTags); toward a
	 * carry receiver the CarryReceiverAllowedTags do not count (carrying a core to the Purification Altar, S7-02).
	 */
	bool IsBlocked(const AActor* Owner, const bool bForReceiver)
	{
		const IGameplayTagAssetInterface* OwnerTags = Cast<IGameplayTagAssetInterface>(Owner);
		if (!OwnerTags)
		{
			return false;
		}
		const UHollowInteractionSettings* Settings = GetDefault<UHollowInteractionSettings>();
		if (!bForReceiver)
		{
			return OwnerTags->HasAnyMatchingGameplayTags(Settings->BlockingTags);
		}
		FGameplayTagContainer Blocking = Settings->BlockingTags;
		Blocking.RemoveTags(Settings->CarryReceiverAllowedTags);
		return OwnerTags->HasAnyMatchingGameplayTags(Blocking);
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

	// The same states the server refuses (dead, carrying a core with both hands...) show no focus and no prompt; carrying
	// still focuses a carry receiver (the Purification Altar, S7-02).
	const UHollowInteractionSettings* Settings = GetDefault<UHollowInteractionSettings>();
	const bool bBlocked = HollowInteractionPrivate::IsBlocked(Owner, /*bForReceiver=*/false);
	if (bBlocked && HollowInteractionPrivate::IsBlocked(Owner, /*bForReceiver=*/true))
	{
		return nullptr;
	}
	const FVector End = EyeLocation + EyeRotation.Vector() * Settings->InteractionRange;
	const FCollisionQueryParams Params = HollowInteractionPrivate::QueryParams(Owner, SCENE_QUERY_STAT_NAME_ONLY(HollowInteractionFocus));

	FHitResult Hit;
	AActor* Focus = nullptr;
	if (World->SweepSingleByChannel(Hit, EyeLocation, End, FQuat::Identity, HollowCollision::Interaction,
		FCollisionShape::MakeSphere(Settings->ProbeRadius), Params))
	{
		AActor* HitActor = Hit.GetActor();
		const IHollowInteractable* Interactable = Cast<IHollowInteractable>(HitActor);
		const bool bAllowed = !bBlocked || Cast<IHollowCarryReceiver>(HitActor) != nullptr;
		Focus = (bAllowed && Interactable && Interactable->CanInteract(Owner)) ? HitActor : nullptr;
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
	const IHollowInteractable* Interactable = Cast<IHollowInteractable>(NewFocus);
	const FHollowInteractionInfo Info = Interactable ? Interactable->GetInteractionInfo(GetOwner()) : FHollowInteractionInfo();
	if (FocusedActor.Get() == NewFocus && Info.Prompt.EqualTo(FocusedPrompt))
	{
		return;
	}

	FocusedActor = NewFocus;
	FocusedPrompt = Info.Prompt;
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
	Check.bInteractorBlocked = HollowInteractionPrivate::IsBlocked(Owner, /*bForReceiver=*/Cast<IHollowCarryReceiver>(Target) != nullptr);
	if (LastInteractionTime.IsSet())
	{
		Check.SecondsSinceLastInteraction = World->GetTimeSeconds() - LastInteractionTime.GetValue();
	}

	// Line of sight from the eyes to the centre of the target's collision bounds: nothing else may block it. The first
	// hit is the target itself, or nothing at all (S7-02: an open shape like the altar, whose centre is in the air).
	FVector EyeLocation;
	FRotator EyeRotation;
	if (HollowInteractionPrivate::GetEyes(Owner, EyeLocation, EyeRotation))
	{
		FVector Origin;
		FVector Extent;
		Target->GetActorBounds(/*bOnlyCollidingComponents=*/true, Origin, Extent);
		const FCollisionQueryParams Params = HollowInteractionPrivate::QueryParams(Owner, SCENE_QUERY_STAT_NAME_ONLY(HollowInteractionValidate));
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, EyeLocation, Origin, HollowCollision::Interaction, Params);
		if (!bBlocked || Hit.GetActor() == Target)
		{
			Check.bHasLineOfSight = true;
			Check.Distance = bBlocked ? Hit.Distance : FVector::Dist(EyeLocation, Origin);
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
