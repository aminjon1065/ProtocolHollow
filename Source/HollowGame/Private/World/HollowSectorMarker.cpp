#include "World/HollowSectorMarker.h"

#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSectorMarker)

AHollowSectorMarker::AHollowSectorMarker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	NetDormancy = DORM_DormantAll;   // sent once to every connection, then silent (ADR-0002)

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // never blocks players, AI or the navmesh
	SetRootComponent(Mesh);
}

void AHollowSectorMarker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHollowSectorMarker, Label, COND_InitialOnly);   // fixed at spawn
}
