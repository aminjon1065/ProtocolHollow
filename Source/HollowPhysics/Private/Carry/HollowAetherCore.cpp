#include "Carry/HollowAetherCore.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagAssetInterface.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAetherCore)

AHollowAetherCore::AHollowAetherCore(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);                 // only while a dropped body is awake (UHollowCarryableComponent)
	NetDormancy = DORM_DormantAll;                 // resting and carried objects send nothing
	bAlwaysRelevant = true;                        // the raid's objective; costs nothing while dormant
	SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionProfileName(HollowCollision::CarryableProfile());
	Body->SetGenerateOverlapEvents(true);          // extraction zones see it, carried or not
	Body->SetCanEverAffectNavigation(false);       // Docs/Architecture/03 sec. 3.6: carried and dropped objects never touch the navmesh
	Body->BodyInstance.bGenerateWakeEvents = true; // the server settles it when it sleeps
	Body->SetSimulatePhysics(false);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Body);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Visual->SetCanEverAffectNavigation(false);

	Carryable = CreateDefaultSubobject<UHollowCarryableComponent>(TEXT("Carryable"));
}

void AHollowAetherCore::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Carryable->SetComponents(Body, Visual);
}

FHollowInteractionInfo AHollowAetherCore::GetInteractionInfo(const AActor* Interactor) const
{
	FHollowInteractionInfo Info;
	if (const UHollowCarryableDefinition* Definition = Carryable->GetDefinition())
	{
		// Someone carries it alone: the other side is free (ADR-0028).
		Info.Prompt = Carryable->GetCarryState().Mode == EHollowCarryMode::Solo ? Definition->JoinPrompt : Definition->PickupPrompt;
	}
	return Info;
}

bool AHollowAetherCore::CanInteract(const AActor* Interactor) const
{
	// Both hands: not while a tool runs (the carry effect blocks tools the other way round).
	const IGameplayTagAssetInterface* InteractorTags = Cast<IGameplayTagAssetInterface>(Interactor);
	const APawn* Pawn = Cast<APawn>(Interactor);
	return (Carryable->CanBeginSolo(Pawn) || Carryable->CanJoin(Pawn))
		&& !(InteractorTags && InteractorTags->HasMatchingGameplayTag(HollowTags::State_Tool_Active));
}

void AHollowAetherCore::Interact(AActor* Interactor)
{
	if (UHollowCarrierComponent* Carrier = Interactor ? Interactor->FindComponentByClass<UHollowCarrierComponent>() : nullptr)
	{
		if (!Carrier->ServerPickUp(Carryable))
		{
			Carrier->ServerJoin(Carryable);   // carried by someone else alone: take the other side
		}
	}
}

void AHollowAetherCore::OnRep_ReplicateMovement()
{
	Super::OnRep_ReplicateMovement();
	Carryable->HandleMovementReplicationChanged();
}

void AHollowAetherCore::BeginPlay()
{
	Super::BeginPlay();
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
	{
		Audio->PlayAttached(HollowTags::Sound_Core_Hum, Visual);   // ends with the core
	}
}
