#include "Carry/HollowAetherCore.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagAssetInterface.h"
#include "Particles/ParticleSystemComponent.h"
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
	DefaultMaterial = Visual->GetMaterial(0);
	Carryable->OnLootItemChanged.AddUObject(this, &ThisClass::ApplyLook);
	Carryable->OnCarryStateChanged.AddWeakLambda(this, [this](const FHollowCarryNetState&, const FHollowCarryNetState&) { ApplyAura(); });
	if (const UHollowCarryableDefinition* Definition = Carryable->GetDefinition())
	{
		TArray<const FHollowFxSpec*> Auras = {&Definition->DefaultAura};
		for (const FHollowCarryableLook& Look : Definition->Looks)
		{
			Auras.Add(&Look.Aura);
		}
		FxHandle = HollowFx::Preload(Auras, FSimpleDelegate::CreateWeakLambda(this, [this] { ApplyAura(); }));
	}
	ApplyLook();   // replicated before BeginPlay (a purified core seen by a late joiner)
}

void AHollowAetherCore::ApplyAura()
{
	const UHollowCarryableDefinition* Definition = Carryable->GetDefinition();
	const FHollowFxSpec* Wanted = Definition ? Definition->FindAura(Carryable->GetLootItem()) : nullptr;
	const UWorld* World = GetWorld();
	const APlayerController* Local = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* LocalPawn = Local && Local->IsLocalController() ? Local->GetPawn() : nullptr;
	if (LocalPawn && Carryable->GetCarryState().IsCarriedBy(LocalPawn))
	{
		Wanted = nullptr;   // the carrier's own first-person view: the core is right in front of the eyes
	}
	if (Wanted == ShownAura && (AuraEffect || !Wanted))
	{
		return;
	}
	HollowFx::Stop(AuraEffect);
	AuraEffect = Wanted ? HollowFx::PlayAttached(*Wanted, Visual) : nullptr;
	ShownAura = AuraEffect ? Wanted : nullptr;   // not loaded yet: the preload's callback tries again
}

void AHollowAetherCore::ApplyLook()
{
	ApplyAura();
	const UHollowCarryableDefinition* Definition = Carryable->GetDefinition();
	const FHollowCarryableLook* Look = Definition ? Definition->FindLook(Carryable->GetLootItem()) : nullptr;
	if (!Look || Look->Material.IsNull())
	{
		Visual->SetMaterial(0, DefaultMaterial);
		return;
	}
	if (UMaterialInterface* Loaded = Look->Material.Get())
	{
		Visual->SetMaterial(0, Loaded);   // already in memory: no frame without it
		return;
	}
	// A small material instance, still loaded without a hitch (CLAUDE.md sec. 2: soft references for assets).
	const TSoftObjectPtr<UMaterialInterface> Material = Look->Material;
	const FPrimaryAssetId Item = Carryable->GetLootItem();
	UAssetManager::GetStreamableManager().RequestAsyncLoad(Material.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, Material, Item]()
	{
		if (Carryable->GetLootItem() == Item)   // still the same item when it arrives
		{
			Visual->SetMaterial(0, Material.Get());
		}
	}));
}
