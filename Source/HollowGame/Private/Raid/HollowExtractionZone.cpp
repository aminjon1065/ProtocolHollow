#include "Raid/HollowExtractionZone.h"

#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowExtractionZone)

AHollowExtractionZone::AHollowExtractionZone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;                 // clients see the pad; the zone itself never changes
	NetDormancy = DORM_DormantAll;
	bAlwaysRelevant = true;             // the visor's EVAC waypoint on every machine, however far (UX-01); dormant: free

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	RootComponent = Volume;
	Volume->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);   // a shape for queries, nothing collides with it
	Volume->SetCanEverAffectNavigation(false);

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	Pad->SetupAttachment(Volume);
	Pad->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Pad->SetCanEverAffectNavigation(false);
}

void AHollowExtractionZone::Sample(TArray<APawn*>& OutPawns, TArray<UHollowCarryableComponent*>& OutCarryables) const
{
	OutPawns.Reset();
	OutCarryables.Reset();
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);   // cores, free or carried (query-only while carried, ADR-0015)
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Volume->GetComponentLocation(), Volume->GetComponentQuat(), Objects, Volume->GetCollisionShape(),
		FCollisionQueryParams(SCENE_QUERY_STAT(HollowExtractionZone), /*bTraceComplex=*/false, this));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			OutPawns.AddUnique(Pawn);
		}
		else if (UHollowCarryableComponent* Carryable = Actor ? Actor->FindComponentByClass<UHollowCarryableComponent>() : nullptr)
		{
			OutCarryables.AddUnique(Carryable);
		}
	}
	// DRONE-01 (ADR-0054): a core the solo drone holds beside its carrier counts with the carrier, wherever the drone's arm is.
	for (const APawn* Pawn : OutPawns)
	{
		const UHollowCarrierComponent* Carrier = Pawn->FindComponentByClass<UHollowCarrierComponent>();
		UHollowCarryableComponent* Carried = Carrier ? Carrier->GetCarried() : nullptr;
		if (Carried && Carried->GetCarryState().Mode == EHollowCarryMode::Drone)
		{
			OutCarryables.AddUnique(Carried);
		}
	}
}
