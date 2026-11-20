#include "Carry/HollowCarryableComponent.h"

#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/ReplicatedState.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HollowPhysicsLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCarryableComponent)

namespace HollowCarryablePrivate
{
	/** Directions tried around the carrier for a drop (every 45 degrees): a search resolution, not tuning. */
	constexpr int32 DropDirections = 8;

	/** Does this machine view the world through Pawn (its first-person camera)? */
	bool IsViewedFirstPerson(const APawn& Pawn)
	{
		// IsPlayerControlled() needs the player state, which a client may receive after the controller.
		return Pawn.IsLocallyControlled() && Cast<APlayerController>(Pawn.GetController()) != nullptr;
	}

	/** The pawns that carry in State (one or two). */
	TArray<APawn*, TInlineAllocator<2>> CarriersOf(const FHollowCarryNetState& State)
	{
		TArray<APawn*, TInlineAllocator<2>> Result;
		if (State.IsCarried())
		{
			for (APawn* Pawn : {State.Primary.Get(), State.Secondary.Get()})
			{
				if (IsValid(Pawn))
				{
					Result.AddUnique(Pawn);
				}
			}
		}
		return Result;
	}
}

UHollowCarryableComponent::UHollowCarryableComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;   // timers and replication only (CLAUDE.md sec. 1)
	SetIsReplicatedByDefault(true);
}

void UHollowCarryableComponent::SetComponents(UPrimitiveComponent* InBody, UPrimitiveComponent* InVisual)
{
	Body = InBody;
	Visual = InVisual;
}

EHollowCarryPresentation UHollowCarryableComponent::GetPresentation() const
{
	if (CarryState.IsCarried() && Visual.IsValid() && Visual->GetAttachParent() != Body.Get())
	{
		return bShownFirstPerson ? EHollowCarryPresentation::FirstPerson : EHollowCarryPresentation::ThirdPerson;
	}
	return Body.IsValid() && Body->IsSimulatingPhysics() ? EHollowCarryPresentation::Awake : EHollowCarryPresentation::Resting;
}

bool UHollowCarryableComponent::CanBeginSolo(const APawn* Carrier) const
{
	const UHollowCarrierComponent* CarrierComponent = IsValid(Carrier) ? Carrier->FindComponentByClass<UHollowCarrierComponent>() : nullptr;
	return Definition && Body.IsValid() && Visual.IsValid() && !CarryState.IsCarried() && CarrierComponent && !CarrierComponent->IsCarrying();
}

void UHollowCarryableComponent::BeginPlay()
{
	Super::BeginPlay();
	UPrimitiveComponent* BodyComponent = Body.Get();
	if (!Definition || !BodyComponent || !Visual.IsValid())
	{
		UE_LOG(LogHollowPhysics, Error, TEXT("%s: carryable needs a definition, a body and a visual."), *GetNameSafe(GetOwner()));
		return;
	}
	VisualRestRelative = Visual->GetRelativeTransform();
	BodyComponent->SetMassOverrideInKg(NAME_None, Definition->Mass, /*bOverrideMass=*/true);
	BodyComponent->SetLinearDamping(Definition->LinearDamping);
	BodyComponent->SetAngularDamping(Definition->AngularDamping);
	BodyComponent->SetPhysMaterialOverride(Definition->PhysicalMaterial);
	if (GetOwner()->HasAuthority())
	{
		BodyComponent->OnComponentSleep.AddDynamic(this, &ThisClass::HandleBodySleepEvent);
		BodyComponent->OnComponentWake.AddDynamic(this, &ThisClass::HandleBodyWakeEvent);
	}
	// A state replicated before BeginPlay (first replication of a carried object) is presented now.
	HandleStateChanged(FHollowCarryNetState());
}

void UHollowCarryableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTimers();
	Super::EndPlay(EndPlayReason);
}

void UHollowCarryableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UHollowCarryableComponent, CarryState, Params);
}

bool UHollowCarryableComponent::BeginSolo(APawn* Carrier)
{
	AActor* Owner = GetOwner();
	if (!Owner->HasAuthority() || !CanBeginSolo(Carrier))
	{
		return false;
	}
	ClearTimers();
	bAwake = false;
	Body->SetSimulatePhysics(false);   // the body is a function of the carrier from now on (ADR-0002)
	Owner->SetReplicateMovement(false);

	FHollowCarryNetState NewState;
	NewState.Mode = EHollowCarryMode::Solo;
	NewState.Primary = Carrier;
	NewState.TransitionSerial = static_cast<uint8>(CarryState.TransitionSerial + 1);
	SetCarryState(NewState);
	Owner->SetNetDormancy(DORM_DormantAll);   // nothing changes while carried: dormant after this update
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: lifted by %s (%s, serial %d)"), *GetNameSafe(Owner), *GetNameSafe(Carrier),
		*HollowEnumToString(CarryState.Mode), CarryState.TransitionSerial);
	return true;
}

bool UHollowCarryableComponent::Release(const FVector& Velocity, const bool bForce)
{
	AActor* Owner = GetOwner();
	const APawn* Carrier = CarryState.Primary.Get();
	if (!Owner->HasAuthority() || !CarryState.IsCarried() || !Definition || !Body.IsValid() || !Carrier)
	{
		return false;
	}
	TOptional<FVector> DropLocation = FindDropLocation(*Carrier);
	if (!DropLocation && !bForce)
	{
		UE_LOG(LogHollowPhysics, Warning, TEXT("%s: no room to put it down around %s; still carried"), *GetNameSafe(Owner), *GetNameSafe(Carrier));
		return false;
	}
	const FVector Location = DropLocation ? DropLocation.GetValue() : GetCarrierCentre(*Carrier);
	const FString CarrierName = GetNameSafe(Carrier);
	Owner->SetNetDormancy(DORM_Awake);
	Owner->SetNetUpdateFrequency(Definition->AwakeNetUpdateFrequency);

	FHollowCarryNetState NewState;
	NewState.TransitionSerial = static_cast<uint8>(CarryState.TransitionSerial + 1);
	SetCarryState(NewState);   // detaches the body where the carrier held it
	Body->SetWorldLocation(Location, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	Body->SetSimulatePhysics(true);
	Body->SetPhysicsLinearVelocity(Velocity);
	Owner->SetReplicateMovement(true);   // Predictive Interpolation while awake (Docs/Architecture/03 sec. 3.0 rule 7)
	Owner->ForceNetUpdate();
	bAwake = true;
	Owner->GetWorldTimerManager().SetTimer(MaxAwakeTimer, this, &ThisClass::ForceSleep, Definition->MaxAwakeSeconds, /*bLoop=*/false);
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: dropped by %s at %s, velocity %s (serial %d)"), *GetNameSafe(Owner), *CarrierName,
		*Owner->GetActorLocation().ToCompactString(), *Velocity.ToCompactString(), CarryState.TransitionSerial);
	return true;
}

TOptional<FVector> UHollowCarryableComponent::FindDropLocation(const APawn& Carrier) const
{
	UWorld* World = GetWorld();
	FCollisionResponseTemplate Free;
	if (!World || !UCollisionProfile::Get()->GetProfileTemplate(HollowCollision::CarryableProfile(), Free))
	{
		return TOptional<FVector>();
	}
	using namespace HollowCarryablePrivate;
	const FTransform& Capsule = Carrier.GetActorTransform();
	const FVector Start = GetCarrierCentre(Carrier);
	const FVector Grip = Definition->GripOffset.GetLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowCarryDrop), /*bTraceComplex=*/false, GetOwner());
	Params.AddIgnoredActor(&Carrier);
	const FCollisionResponseParams Responses(Free.ResponseToChannels);
	const FQuat Rotation = Body->GetComponentQuat();
	const FCollisionShape Shape = Body->GetCollisionShape();
	for (int32 Step = 0; Step < DropDirections; ++Step)
	{
		// 0, +45, -45, +90, -90, +135, -135, 180 degrees: front first, then ever further round to the back.
		const int32 Turn = (Step + 1) / 2 * (Step % 2 == 1 ? 1 : -1);
		const FVector Candidate = Capsule.TransformPosition(Grip.RotateAngleAxis(Turn * 360.f / DropDirections, FVector::UpVector));
		// The spot must be free, and nothing may stand between the carrier's centre and it. A line, not a sweep: the
		// body turned with the carrier may graze the walls its capsule touches (a corner), which a sweep from there
		// reports as blocking even when moving away; the centre itself is never inside a wall.
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, Candidate, Rotation, Free.ObjectType, Shape, Params, Responses);
		const FOverlapResult* Inside = Overlaps.FindByPredicate([](const FOverlapResult& Overlap) { return Overlap.bBlockingHit; });
		FHitResult Hit;
		const bool bBetween = !Inside && World->LineTraceSingleByChannel(Hit, Start, Candidate, Free.ObjectType, Params, Responses);
		UE_LOG(LogHollowPhysics, Verbose, TEXT("%s: drop %d degrees round: inside %s, in the way %s"), *GetNameSafe(GetOwner()), Turn * 360 / DropDirections,
			Inside ? *GetNameSafe(Inside->GetActor()) : TEXT("nothing"), bBetween ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"));
		if (!Inside && !bBetween)
		{
			UE_CLOG(Step > 0, LogHollowPhysics, Log, TEXT("%s: no room in front of %s, put down %d degrees round"), *GetNameSafe(GetOwner()),
				*GetNameSafe(&Carrier), Turn * 360 / DropDirections);
			return Candidate;
		}
	}
	return TOptional<FVector>();
}

FVector UHollowCarryableComponent::GetCarrierCentre(const APawn& Carrier) const
{
	return Carrier.GetActorTransform().TransformPosition(FVector(0.0, 0.0, Definition->GripOffset.GetLocation().Z));
}

void UHollowCarryableComponent::SnapToReplicatedPose()
{
	AActor* Owner = GetOwner();
	const FRepMovement& Replicated = Owner->GetReplicatedMovement();
	const FVector Location = FRepMovement::RebaseOntoLocalOrigin(Replicated.Location, Owner);
	const FVector Before = Owner->GetActorLocation();
	Owner->SetActorLocationAndRotation(Location, Replicated.Rotation, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: on the replicated pose %s (local pose was %.1f cm off)"), *GetNameSafe(Owner), *Location.ToCompactString(),
		FVector::Dist(Before, Location));
}

void UHollowCarryableComponent::HandleBodySleep()
{
	AActor* Owner = GetOwner();
	if (!Owner->HasAuthority() || !bAwake || CarryState.IsCarried() || !Definition || Owner->GetWorldTimerManager().IsTimerActive(DormancyTimer))
	{
		return;
	}
	Owner->ForceNetUpdate();   // the final pose, with the sleeping flag
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: asleep at %s"), *GetNameSafe(Owner), *Owner->GetActorLocation().ToCompactString());
	Owner->GetWorldTimerManager().SetTimer(DormancyTimer, this, &ThisClass::SettleAtRest,
		FMath::Max(Definition->DormancyDelaySeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

void UHollowCarryableComponent::HandleBodySleepEvent(UPrimitiveComponent* SleepingComponent, FName BoneName)
{
	HandleBodySleep();
}

void UHollowCarryableComponent::HandleBodyWakeEvent(UPrimitiveComponent* WakingComponent, FName BoneName)
{
	// Moving again before it settled (hit, landed on a slope): wait for the next sleep, and keep the awake limit.
	FTimerManager& Timers = GetOwner()->GetWorldTimerManager();
	Timers.ClearTimer(DormancyTimer);
	if (bAwake && Definition && !Timers.IsTimerActive(MaxAwakeTimer))
	{
		Timers.SetTimer(MaxAwakeTimer, this, &ThisClass::ForceSleep, Definition->MaxAwakeSeconds, /*bLoop=*/false);
	}
}

void UHollowCarryableComponent::ForceSleep()
{
	if (Body.IsValid() && bAwake)
	{
		UE_LOG(LogHollowPhysics, Log, TEXT("%s: still awake after %.1f s, put to sleep"), *GetNameSafe(GetOwner()), Definition->MaxAwakeSeconds);
		Body->PutAllRigidBodiesToSleep();
		HandleBodySleep();
	}
}

void UHollowCarryableComponent::SettleAtRest()
{
	AActor* Owner = GetOwner();
	ClearTimers();
	if (!bAwake || CarryState.IsCarried() || !Body.IsValid())
	{
		return;
	}
	bAwake = false;
	Body->SetSimulatePhysics(false);   // kinematic where it lies: nothing can push it apart on one machine
	Owner->SetReplicateMovement(false);
	Owner->ForceNetUpdate();
	Owner->SetNetDormancy(DORM_DormantAll);
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: at rest at %s (dormant)"), *GetNameSafe(Owner), *Owner->GetActorLocation().ToCompactString());
}

void UHollowCarryableComponent::HandleMovementReplicationChanged()
{
	AActor* Owner = GetOwner();
	if (Owner->HasAuthority() || Owner->IsReplicatingMovement() || CarryState.IsCarried() || !Body.IsValid())
	{
		return;
	}
	// The server put the object to rest: stop simulating exactly on its last replicated pose, the same on every machine.
	Body->SetSimulatePhysics(false);
	SnapToReplicatedPose();
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: at rest (replicated)"), *GetNameSafe(Owner));
}

void UHollowCarryableComponent::HandleCarrierEndPlay(const APawn* Carrier)
{
	if (!GetOwner()->HasAuthority() && CarryState.IsCarriedBy(Carrier) && HasBegunPlay())
	{
		ApplyPresentation(Carrier);
	}
}

void UHollowCarryableComponent::SetCarryState(const FHollowCarryNetState& NewState)
{
	const FHollowCarryNetState OldState = CarryState;
	CarryState = NewState;
	// Dormant between changes (ADR-0002): wake, mark, send now.
	GetOwner()->FlushNetDormancy();
	MARK_PROPERTY_DIRTY_FROM_NAME(UHollowCarryableComponent, CarryState, this);
	HandleStateChanged(OldState);
	GetOwner()->ForceNetUpdate();
}

void UHollowCarryableComponent::OnRep_CarryState(const FHollowCarryNetState& OldState)
{
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: carry %s by %s (serial %d, replicated)"), *GetNameSafe(GetOwner()), *HollowEnumToString(CarryState.Mode),
		*GetNameSafe(CarryState.Primary), CarryState.TransitionSerial);
	HandleStateChanged(OldState);
}

void UHollowCarryableComponent::HandleStateChanged(const FHollowCarryNetState& OldState)
{
	if (!HasBegunPlay())
	{
		return;   // first replication before BeginPlay: BeginPlay presents it
	}
	using namespace HollowCarryablePrivate;
	const TArray<APawn*, TInlineAllocator<2>> NewCarriers = CarriersOf(CarryState);
	for (APawn* Pawn : CarriersOf(OldState))
	{
		UHollowCarrierComponent* Carrier = Pawn->FindComponentByClass<UHollowCarrierComponent>();
		if (Carrier && !NewCarriers.Contains(Pawn))
		{
			Carrier->NotifyCarryChanged(this, /*bCarrying=*/false);
		}
	}
	for (APawn* Pawn : NewCarriers)
	{
		if (UHollowCarrierComponent* Carrier = Pawn->FindComponentByClass<UHollowCarrierComponent>())
		{
			Carrier->NotifyCarryChanged(this, /*bCarrying=*/true);
		}
	}
	ApplyPresentation();
	if (!GetOwner()->HasAuthority() && OldState.IsCarried() && !CarryState.IsCarried() && GetOwner()->IsReplicatingMovement())
	{
		// Dropped: start from the server's drop pose (it arrives with this state), not from where this machine held it.
		SnapToReplicatedPose();
	}
	OnCarryStateChanged.Broadcast(OldState, CarryState);
}

void UHollowCarryableComponent::ApplyPresentation(const APawn* IgnoredCarrier)
{
	UPrimitiveComponent* BodyComponent = Body.Get();
	UPrimitiveComponent* VisualComponent = Visual.Get();
	if (!Definition || !BodyComponent || !VisualComponent)
	{
		return;
	}
	APawn* Carrier = CarryState.IsCarried() ? CarryState.Primary.Get() : nullptr;
	const UHollowCarrierComponent* CarrierComponent = IsValid(Carrier) && Carrier != IgnoredCarrier ? Carrier->FindComponentByClass<UHollowCarrierComponent>() : nullptr;
	if (CarrierComponent && Carrier->GetRootComponent())
	{
		// The body rides on the capsule: a function of the carrier only, identical on every machine.
		BodyComponent->SetSimulatePhysics(false);
		BodyComponent->SetCollisionProfileName(HollowCollision::CarriedProfile());
		USceneComponent* Capsule = Carrier->GetRootComponent();
		if (BodyComponent->GetAttachParent() != Capsule)
		{
			BodyComponent->AttachToComponent(Capsule, FAttachmentTransformRules::KeepWorldTransform);
		}
		BodyComponent->SetRelativeLocationAndRotation(Definition->GripOffset.GetLocation(), Definition->GripOffset.GetRotation());

		// The visual follows what this machine renders: the own first-person view, or someone else's smoothed body.
		const bool bFirstPerson = HollowCarryablePrivate::IsViewedFirstPerson(*Carrier);
		const FHollowCarryAttach& Attach = bFirstPerson ? Definition->FirstPerson : Definition->ThirdPerson;
		USceneComponent* Target = CarrierComponent->GetAttachTarget(bFirstPerson);
		if (Target)
		{
			VisualComponent->AttachToComponent(Target, FAttachmentTransformRules::KeepRelativeTransform, Attach.Socket);
			VisualComponent->SetRelativeTransform(VisualRestRelative * Attach.Offset);
		}
		VisualComponent->SetFirstPersonPrimitiveType(bFirstPerson && Target ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
		bShownFirstPerson = bFirstPerson && Target;
		return;
	}

	if (BodyComponent->GetAttachParent())
	{
		BodyComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);   // put down where it was held
	}
	BodyComponent->SetCollisionProfileName(HollowCollision::CarryableProfile());
	if (VisualComponent->GetAttachParent() != BodyComponent)
	{
		VisualComponent->AttachToComponent(BodyComponent, FAttachmentTransformRules::KeepRelativeTransform);
	}
	VisualComponent->SetRelativeTransform(VisualRestRelative);
	VisualComponent->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
	bShownFirstPerson = false;
}

void UHollowCarryableComponent::ClearTimers()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DormancyTimer);
		World->GetTimerManager().ClearTimer(MaxAwakeTimer);
	}
}
