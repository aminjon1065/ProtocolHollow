#include "World/HollowLootPickup.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HollowGameLog.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Items/HollowItemDefinition.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLootPickup)

#define LOCTEXT_NAMESPACE "HollowLootPickup"

namespace HollowLootPickupPrivate
{
	/** Picked up: the server keeps it this long so the pick replicates before the actor goes. */
	constexpr float GoneSeconds = 0.25f;
}

AHollowLootPickup::AHollowLootPickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);   // while it moves; dormant once it rests
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetCollisionProfileName(HollowCollision::PickupProfile());
	Body->SetCanEverAffectNavigation(false);
	Body->SetSimulatePhysics(true);
	SetRootComponent(Body);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
}

void AHollowLootPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowLootPickup, Item, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowLootPickup, Count, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowLootPickup, bTaken, Params);
}

void AHollowLootPickup::SetLoot(UHollowItemDefinition* InItem, const int32 InCount)
{
	Item = InItem;
	Count = FMath::Max(InCount, 0);
	MARK_PROPERTY_DIRTY_FROM_NAME(AHollowLootPickup, Item, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AHollowLootPickup, Count, this);
}

void AHollowLootPickup::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RestTimer, this, &ThisClass::CheckRest, RestCheckSeconds, /*bLoop=*/true);
	}
}

void AHollowLootPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LookLoad.IsValid())
	{
		LookLoad->CancelHandle();
		LookLoad.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void AHollowLootPickup::Launch(const FVector& Velocity)
{
	if (HasAuthority() && Body->IsSimulatingPhysics())
	{
		Body->SetPhysicsLinearVelocity(Velocity);
	}
}

void AHollowLootPickup::CheckRest()
{
	if (!Body->IsAnyRigidBodyAwake())
	{
		GetWorldTimerManager().ClearTimer(RestTimer);
		SetNetDormancy(DORM_DormantAll);   // at rest: nothing more to send until someone picks it up
	}
}

void AHollowLootPickup::OnRep_Loot()
{
	ApplyLook();
}

void AHollowLootPickup::ApplyLook()
{
	if (!Item || Item->PickupMesh.IsNull() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UStaticMesh* Loaded = Item->PickupMesh.Get())
	{
		Mesh->SetStaticMesh(Loaded);
		Mesh->SetRelativeScale3D(FVector::OneVector);   // the item's own mesh at its size (the class's box is scaled)
		return;
	}
	LookLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Item->PickupMesh.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this]
	{
		LookLoad.Reset();
		if (Item && Item->PickupMesh.Get())
		{
			Mesh->SetStaticMesh(Item->PickupMesh.Get());
			Mesh->SetRelativeScale3D(FVector::OneVector);
		}
	}));
}

FHollowInteractionInfo AHollowLootPickup::GetInteractionInfo(const AActor* Interactor) const
{
	FHollowInteractionInfo Info;
	Info.Prompt = Item ? FText::Format(LOCTEXT("PickUp", "Pick up {0} x{1}"), Item->DisplayName, Count) : FText::GetEmpty();
	return Info;
}

bool AHollowLootPickup::CanInteract(const AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	return !bTaken && Item && Count > 0 && Pawn && UHollowInventoryComponent::Get(Pawn->GetPlayerState());
}

void AHollowLootPickup::Interact(AActor* Interactor)
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	UHollowInventoryComponent* Inventory = Pawn ? UHollowInventoryComponent::Get(Pawn->GetPlayerState()) : nullptr;
	if (!HasAuthority() || !CanInteract(Interactor) || !Inventory)
	{
		return;
	}
	Inventory->AddFoundItem(Item, Count);
	FlushNetDormancy();
	bTaken = true;
	MARK_PROPERTY_DIRTY_FROM_NAME(AHollowLootPickup, bTaken, this);
	ForceNetUpdate();
	OnRep_Taken();
	SetLifeSpan(HollowLootPickupPrivate::GoneSeconds);
	UE_LOG(LogHollowGame, Log, TEXT("%s: %s picks up %d %s"), *GetName(), *GetNameSafe(Interactor), Count, *Item->GetName());
}

void AHollowLootPickup::OnRep_Taken()
{
	if (!bTaken)
	{
		return;
	}
	Mesh->SetVisibility(false);
	Body->SetSimulatePhysics(false);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
	{
		Audio->Play(HollowTags::Sound_Loot_Pickup, GetActorLocation());
	}
}

#undef LOCTEXT_NAMESPACE
