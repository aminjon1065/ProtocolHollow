#include "World/HollowDatapad.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "HollowGameLog.h"
#include "Lore/HollowLoreSubsystem.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDatapad)

namespace HollowDatapadPrivate
{
	/** Downloaded: the server keeps it this long so the download replicates before the actor goes. */
	constexpr float GoneSeconds = 0.25f;
	/** What the interaction trace aims at: a little more than the pad itself (cm, half extents). */
	const FVector FocusExtent(14.0, 10.0, 6.0);
	/** Nothing about a pad on the floor changes until it is downloaded. */
	constexpr float NetUpdateRate = 2.f;
}

AHollowDatapad::AHollowDatapad(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetNetUpdateFrequency(HollowDatapadPrivate::NetUpdateRate);
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetBoxExtent(HollowDatapadPrivate::FocusExtent);
	Body->SetCollisionProfileName(HollowCollision::PickupProfile());
	Body->SetCanEverAffectNavigation(false);
	SetRootComponent(Body);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
}

void AHollowDatapad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowDatapad, bTaken, Params);
}

FHollowInteractionInfo AHollowDatapad::GetInteractionInfo(const AActor* Interactor) const
{
	FHollowInteractionInfo Info;
	Info.Prompt = Prompt;
	return Info;
}

bool AHollowDatapad::CanInteract(const AActor* Interactor) const
{
	return !bTaken && Cast<APawn>(Interactor);
}

void AHollowDatapad::Interact(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract(Interactor))
	{
		return;
	}
	UHollowLoreSubsystem* Lore = UHollowLoreSubsystem::Get(GetWorld());
	if (!Lore || !Lore->Download(Interactor).IsValid())
	{
		return;   // nothing to give (said why): the pad stays
	}
	bTaken = true;
	MARK_PROPERTY_DIRTY_FROM_NAME(AHollowDatapad, bTaken, this);
	ForceNetUpdate();
	OnRep_Taken();
	SetLifeSpan(HollowDatapadPrivate::GoneSeconds);
	UE_LOG(LogHollowGame, Log, TEXT("%s: downloaded by %s"), *GetName(), *GetNameSafe(Interactor));
}

void AHollowDatapad::OnRep_Taken()
{
	if (!bTaken)
	{
		return;
	}
	Mesh->SetVisibility(false);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
	{
		Audio->Play(HollowTags::Sound_Loot_Pickup, GetActorLocation());
	}
}
