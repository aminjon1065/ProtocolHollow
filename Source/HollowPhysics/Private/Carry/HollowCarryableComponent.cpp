#include "Carry/HollowCarryableComponent.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarrySettings.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/ReplicatedState.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "HollowPhysicsLog.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Tags/HollowGameplayTags.h"
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

	/**
	 * Where this machine renders Pawn's capsule: a character's mesh carries the network smoothing of a remote pawn
	 * (its offset from the base translation), so the co-op visual stays in the carriers' hands as they are drawn.
	 */
	FTransform RenderedCapsule(const APawn& Pawn)
	{
		FTransform Capsule = Pawn.GetActorTransform();
		const ACharacter* Character = Cast<ACharacter>(&Pawn);
		if (const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr)
		{
			Capsule.AddToTranslation(Capsule.TransformVectorNoScale(Mesh->GetRelativeLocation() - Character->GetBaseTranslationOffset()));
		}
		Capsule.SetScale3D(FVector::OneVector);
		return Capsule;
	}

	/** The component whose moves move Pawn as rendered: a character's mesh (smoothing included), else its root. */
	USceneComponent* PoseAnchor(APawn& Pawn)
	{
		ACharacter* Character = Cast<ACharacter>(&Pawn);
		USceneComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		return Mesh ? Mesh : Pawn.GetRootComponent();
	}

	UHollowCharacterMovementComponent* MovementOf(const APawn& Pawn)
	{
		const ACharacter* Character = Cast<ACharacter>(&Pawn);
		return Character ? Cast<UHollowCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
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
	if (CarryState.IsCarried() && bShownCoop)
	{
		return EHollowCarryPresentation::Coop;
	}
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
	UnbindCoopPose();
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

bool UHollowCarryableComponent::CanJoin(const APawn* Pawn) const
{
	const UHollowCarrierComponent* CarrierComponent = IsValid(Pawn) ? Pawn->FindComponentByClass<UHollowCarrierComponent>() : nullptr;
	const APawn* Primary = CarryState.Primary.Get();
	return Definition && Definition->CoopEffect && Body.IsValid() && Visual.IsValid() && CarryState.Mode == EHollowCarryMode::Solo
		&& IsValid(Primary) && Pawn != Primary && CarrierComponent && !CarrierComponent->IsCarrying()
		&& !HollowCarryTether::IsBroken(Primary->GetActorLocation(), Pawn->GetActorLocation(), Definition->Coop);
}

bool UHollowCarryableComponent::BeginCoop(APawn* Secondary)
{
	AActor* Owner = GetOwner();
	if (!Owner->HasAuthority() || !CanJoin(Secondary))
	{
		return false;
	}
	FHollowCarryNetState NewState = CarryState;
	NewState.Mode = EHollowCarryMode::Coop;
	NewState.Secondary = Secondary;
	NewState.TransitionSerial = static_cast<uint8>(CarryState.TransitionSerial + 1);
	SetCarryState(NewState);
	Owner->SetNetDormancy(DORM_DormantAll);
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: %s joins %s (Coop, serial %d)"), *GetNameSafe(Owner), *GetNameSafe(Secondary), *GetNameSafe(CarryState.Primary),
		CarryState.TransitionSerial);
	return true;
}

bool UHollowCarryableComponent::ReleaseCarrier(APawn* Carrier, const EHollowCarryLeave Reason)
{
	AActor* Owner = GetOwner();
	if (!Owner->HasAuthority() || CarryState.Mode != EHollowCarryMode::Coop || !CarryState.IsCarriedBy(Carrier))
	{
		return false;
	}
	APawn* Remaining = CarryState.Primary == Carrier ? CarryState.Secondary.Get() : CarryState.Primary.Get();
	if (!IsValid(Remaining))
	{
		return Release(FVector::ZeroVector, /*bForce=*/true);   // both are gone
	}
	FHollowCarryNetState NewState;
	NewState.Mode = EHollowCarryMode::Solo;
	NewState.Primary = Remaining;
	NewState.TransitionSerial = static_cast<uint8>(CarryState.TransitionSerial + 1);
	SetCarryState(NewState);
	Owner->SetNetDormancy(DORM_DormantAll);
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: %s lets go (%s); %s carries it alone (serial %d)"), *GetNameSafe(Owner), *GetNameSafe(Carrier),
		*HollowEnumToString(Reason), *GetNameSafe(Remaining), CarryState.TransitionSerial);
	return true;
}

bool UHollowCarryableComponent::Release(const FVector& Velocity, const bool bForce)
{
	AActor* Owner = GetOwner();
	// Put down by whoever is still there (carried by two, the primary may already be gone).
	APawn* Carrier = IsValid(CarryState.Primary) ? CarryState.Primary.Get() : CarryState.Secondary.Get();
	if (!Owner->HasAuthority() || !CarryState.IsCarried() || !Definition || !Body.IsValid() || !IsValid(Carrier))
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
	APawn* DroppedBy = Carrier;   // the carry state below forgets the carrier
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
	// A heavy core hitting the deck (ADR-0018): the carrier made the noise.
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this))
	{
		Noise->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Core_Drop, Location, DroppedBy));
	}
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
	if (GetOwner()->HasAuthority())
	{
		UpdateSanityTimer();
	}
	if (OldState.IsCarried() && !CarryState.IsCarried())
	{
		if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
		{
			Audio->Play(HollowTags::Noise_Core_Drop, GetOwner()->GetActorLocation());   // the thud the AI heard
		}
	}
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
	// The carriers this machine shows: a leaving one (IgnoredCarrier) is shown gone until the server's state arrives.
	TArray<APawn*, TInlineAllocator<2>> Shown = HollowCarryablePrivate::CarriersOf(CarryState);
	Shown.RemoveAll([IgnoredCarrier](const APawn* Pawn)
	{
		return Pawn == IgnoredCarrier || !Pawn->GetRootComponent() || !Pawn->FindComponentByClass<UHollowCarrierComponent>();
	});
	UnbindCoopPose();
	VisualComponent->SetUsingAbsoluteLocation(false);
	VisualComponent->SetUsingAbsoluteRotation(false);
	bShownCoop = false;

	if (Shown.Num() > 0)
	{
		// The body rides on the (first) carrier's capsule: a function of the carriers only, identical on every machine.
		APawn* Carrier = Shown[0];
		BodyComponent->SetSimulatePhysics(false);
		BodyComponent->SetCollisionProfileName(HollowCollision::CarriedProfile());
		USceneComponent* Capsule = Carrier->GetRootComponent();
		if (BodyComponent->GetAttachParent() != Capsule)
		{
			BodyComponent->AttachToComponent(Capsule, FAttachmentTransformRules::KeepWorldTransform);
		}
		BodyComponent->SetRelativeLocationAndRotation(Definition->GripOffset.GetLocation(), Definition->GripOffset.GetRotation());

		if (CarryState.Mode == EHollowCarryMode::Coop && Shown.Num() == 2)
		{
			// Between two (ADR-0028): a world object on every machine, placed whenever either carrier moves.
			if (VisualComponent->GetAttachParent() != BodyComponent)
			{
				VisualComponent->AttachToComponent(BodyComponent, FAttachmentTransformRules::KeepRelativeTransform);
			}
			VisualComponent->SetRelativeTransform(VisualRestRelative);
			VisualComponent->SetUsingAbsoluteLocation(true);
			VisualComponent->SetUsingAbsoluteRotation(true);
			VisualComponent->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
			bShownFirstPerson = false;
			bShownCoop = true;
			BindCoopPose(*Shown[0], *Shown[1]);
			UpdateCoopPose();
			return;
		}

		// The visual follows what this machine renders: the own first-person view, or someone else's smoothed body.
		const UHollowCarrierComponent* CarrierComponent = Carrier->FindComponentByClass<UHollowCarrierComponent>();
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

void UHollowCarryableComponent::UpdateCoopPose()
{
	const APawn* Primary = CarryState.Primary.Get();
	const APawn* Secondary = CarryState.Secondary.Get();
	UPrimitiveComponent* BodyComponent = Body.Get();
	UPrimitiveComponent* VisualComponent = Visual.Get();
	if (!bShownCoop || !IsValid(Primary) || !IsValid(Secondary) || !BodyComponent || !VisualComponent || !Definition)
	{
		return;
	}
	using namespace HollowCarryablePrivate;
	const double GripHeight = Definition->GripOffset.GetLocation().Z;
	FTransform Capsules[2] = {Primary->GetActorTransform(), Secondary->GetActorTransform()};
	for (FTransform& Capsule : Capsules)
	{
		Capsule.SetScale3D(FVector::OneVector);
	}
	const FTransform Truth = HollowCarryCoop::Pose(Capsules[0], Capsules[1], GripHeight);
	BodyComponent->SetWorldLocationAndRotation(Truth.GetLocation(), Truth.GetRotation());
	const FTransform Rendered = VisualRestRelative * HollowCarryCoop::Pose(RenderedCapsule(*Primary), RenderedCapsule(*Secondary), GripHeight);
	VisualComponent->SetWorldLocationAndRotation(Rendered.GetLocation(), Rendered.GetRotation());
}

void UHollowCarryableComponent::HandleCarrierMoved(USceneComponent* Component, EUpdateTransformFlags Flags, ETeleportType Teleport)
{
	UpdateCoopPose();
}

void UHollowCarryableComponent::BindCoopPose(APawn& Primary, APawn& Secondary)
{
	APawn* Carriers[2] = {&Primary, &Secondary};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (USceneComponent* Anchor = HollowCarryablePrivate::PoseAnchor(*Carriers[Index]))
		{
			PoseAnchors[Index] = Anchor;
			PoseHandles[Index] = Anchor->TransformUpdated.AddUObject(this, &ThisClass::HandleCarrierMoved);
		}
	}
}

void UHollowCarryableComponent::UnbindCoopPose()
{
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (USceneComponent* Anchor = PoseAnchors[Index].Get())
		{
			Anchor->TransformUpdated.Remove(PoseHandles[Index]);
		}
		PoseAnchors[Index] = nullptr;
		PoseHandles[Index].Reset();
	}
}

void UHollowCarryableComponent::UpdateSanityTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerManager& Timers = World->GetTimerManager();
	const bool bCoop = CarryState.Mode == EHollowCarryMode::Coop && IsValid(CarryState.Primary) && IsValid(CarryState.Secondary);
	if (!bCoop)
	{
		Timers.ClearTimer(SanityTimer);
		return;
	}
	if (Timers.IsTimerActive(SanityTimer))
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const APawn* Carriers[2] = {CarryState.Primary.Get(), CarryState.Secondary.Get()};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		SanitySamples[Index] = SampleOf(*Carriers[Index], Now);
	}
	Timers.SetTimer(SanityTimer, this, &ThisClass::CheckCoopSanity, GetDefault<UHollowCarrySettings>()->CoopSanityIntervalSeconds, /*bLoop=*/true);
}

UHollowCarryableComponent::FSanitySample UHollowCarryableComponent::SampleOf(const APawn& Carrier, const double Now) const
{
	FSanitySample Sample;
	Sample.Location = Carrier.GetActorLocation();
	Sample.Time = Now;
	if (const UHollowCharacterMovementComponent* Movement = HollowCarryablePrivate::MovementOf(Carrier))
	{
		Sample.ClientTime = Movement->GetClientTimeStamp();
		// A client-authoritative carrier: its own claims, on its own clock (between them the server simulates it itself).
		Movement->GetAcceptedClientMove(Sample.Location, Sample.ClientTime);
	}
	return Sample;
}

bool UHollowCarryableComponent::IsThroughWorld(const APawn& Carrier, const FVector& From, const FVector& Location) const
{
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Carrier.GetRootComponent());
	UWorld* World = GetWorld();
	if (!Capsule || !World)
	{
		return false;
	}
	// What blocks the carrier's capsule, but not other pawns (they shove each other) or the carried object itself.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowCoopSanity), /*bTraceComplex=*/false, &Carrier);
	FCollisionResponseParams Responses;
	Capsule->InitSweepCollisionParams(Params, Responses);
	Params.AddIgnoredActor(GetOwner());
	Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	const float Shrink = GetDefault<UHollowCarrySettings>()->CoopPenetrationTolerance;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(Capsule->GetScaledCapsuleRadius() - Shrink, 1.f),
		FMath::Max(Capsule->GetScaledCapsuleHalfHeight() - Shrink, 1.f));
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	// Inside something now, or a wall between where it last passed and here (a line from centre to centre: the path
	// of a capsule that walked round a corner stays clear of the walls it passed).
	return World->OverlapBlockingTestByChannel(Location, Capsule->GetComponentQuat(), Channel, Shape, Params, Responses)
		|| World->LineTraceTestByChannel(From, Location, Channel, Params, Responses);
}

void UHollowCarryableComponent::CheckCoopSanity()
{
	APawn* Carriers[2] = {CarryState.Primary.Get(), CarryState.Secondary.Get()};
	UWorld* World = GetWorld();
	if (!GetOwner()->HasAuthority() || CarryState.Mode != EHollowCarryMode::Coop || !IsValid(Carriers[0]) || !IsValid(Carriers[1]) || !World || !Definition)
	{
		UpdateSanityTimer();
		return;
	}
	const UHollowCarrySettings* Settings = GetDefault<UHollowCarrySettings>();
	const double Now = World->GetTimeSeconds();
	FSanitySample Current[2];
	FVector Locations[2];
	double Moved[2];
	bool bViolated[2] = {false, false};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const APawn& Carrier = *Carriers[Index];
		const FSanitySample& Sample = SanitySamples[Index];
		Current[Index] = SampleOf(Carrier, Now);
		Locations[Index] = Carrier.GetActorLocation();
		Moved[Index] = FVector::Dist(Sample.Location, Current[Index].Location);
		// Only positions the server took from a client need checking: everyone else moved under its authority.
		const UHollowCharacterMovementComponent* Movement = HollowCarryablePrivate::MovementOf(Carrier);
		if (!Movement || !Movement->IsClientAuthoritative())
		{
			continue;
		}
		const FVector& Claimed = Current[Index].Location;
		const double MovedFor = HollowCoopSanity::MovedFor(Now - Sample.Time, Current[Index].ClientTime - Sample.ClientTime);
		const bool bTooFast = HollowCoopSanity::IsTooFast(Sample.Location, Claimed, MovedFor,
			Movement->GetCarryWalkSpeed(EHollowCarryClaim::Coop), Settings->CoopSpeedTolerance);
		const bool bThroughWorld = IsThroughWorld(Carrier, Sample.Location, Claimed);
		bViolated[Index] = bTooFast || bThroughWorld;
		UE_CLOG(bViolated[Index], LogHollowPhysics, Warning, TEXT("%s: sanity check refuses %s (%s%s): %s -> %s in %.2f s"), *GetNameSafe(GetOwner()),
			*GetNameSafe(&Carrier), bTooFast ? TEXT("too fast") : TEXT(""), bThroughWorld ? (bTooFast ? TEXT(", through the world") : TEXT("through the world")) : TEXT(""),
			*Sample.Location.ToCompactString(), *Claimed.ToCompactString(), MovedFor);
	}

	if (bViolated[0] || bViolated[1])
	{
		// Refused carriers go back where they last passed (one correction each) and let go; both refused: it drops.
		for (int32 Index = 0; Index < 2; ++Index)
		{
			if (bViolated[Index])
			{
				HollowCarryablePrivate::MovementOf(*Carriers[Index])->ForceServerAuthority(SanitySamples[Index].Location);
			}
		}
		if (bViolated[0] && bViolated[1])
		{
			Release(FVector::ZeroVector, /*bForce=*/true);
		}
		else
		{
			ReleaseCarrier(Carriers[bViolated[0] ? 0 : 1], EHollowCarryLeave::Violation);
		}
		return;
	}
	if (HollowCarryTether::IsBroken(Locations[0], Locations[1], Definition->Coop))
	{
		const bool bSecondaryLetsGo = HollowCoopSanity::DoesSecondaryLetGo(Moved[0], Moved[1]);
		UE_LOG(LogHollowPhysics, Log, TEXT("%s: the tether breaks: %.0f cm apart (at most %.0f)"), *GetNameSafe(GetOwner()),
			FVector::Dist(Locations[0], Locations[1]), Definition->Coop.GetMaxSeparation());
		ReleaseCarrier(Carriers[bSecondaryLetsGo ? 1 : 0], EHollowCarryLeave::TetherBroken);
		return;
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		SanitySamples[Index] = Current[Index];
	}
}

void UHollowCarryableComponent::ClearTimers()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DormancyTimer);
		World->GetTimerManager().ClearTimer(MaxAwakeTimer);
		World->GetTimerManager().ClearTimer(SanityTimer);
	}
}
