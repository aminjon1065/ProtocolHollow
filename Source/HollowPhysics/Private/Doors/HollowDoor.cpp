#include "Doors/HollowDoor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Collision/HollowCollisionChannels.h"
#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Nav/HollowDoorNav.h"
#include "Nav/HollowNavUpdateSubsystem.h"
#include "NavAreas/NavArea_Null.h"
#include "NavLinkCustomComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDoor)

AHollowDoor::AHollowDoor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	NetDormancy = DORM_DormantAll;   // server-spawned: sent once, then only on FlushNetDormancy (ADR-0002)

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));
	Leaf->SetupAttachment(Root);
	Leaf->SetCanEverAffectNavigation(false);   // doors reach navigation through links (S2-05), not through tile rebuilds

	Doorway = CreateDefaultSubobject<UBoxComponent>(TEXT("Doorway"));
	Doorway->SetupAttachment(Root);
	Doorway->SetCanEverAffectNavigation(false);
	Doorway->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Doorway->SetCollisionResponseToAllChannels(ECR_Ignore);
	Doorway->SetCollisionResponseToChannel(HollowCollision::Interaction, ECR_Block);
	Doorway->SetGenerateOverlapEvents(false);   // occupancy is an explicit query at decision time (IsDoorwayClear)

	DoorState = CreateDefaultSubobject<UHollowDoorStateComponent>(TEXT("DoorState"));

	NavCut = CreateDefaultSubobject<UBoxComponent>(TEXT("NavCut"));
	NavCut->SetupAttachment(Root);
	NavCut->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavCut->SetCanEverAffectNavigation(true);
	NavCut->SetCustomNavigableGeometry(EHasCustomNavigableGeometry::EvenIfNotCollidable);   // relevant without blocking anyone
	NavCut->bDynamicObstacle = true;
	NavCut->SetAreaClassOverride(UNavArea_Null::StaticClass());

	NavLink = CreateDefaultSubobject<UNavLinkCustomComponent>(TEXT("NavLink"));
}

void AHollowDoor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Before the sector's navmesh is built (the server spawns doors first), so the link is part of it from the start.
	// Area first: SetLinkData refreshes the link's navigation data, so the octree copy starts with this area.
	if (const UHollowDoorDefinition* Definition = DoorState->GetDefinition())
	{
		FHollowDoorNetState Initial;
		Initial.State = Definition->InitialState;
		NavArea = HollowDoorNav::AreaFor(Initial, 0.f, *Definition);
		NavLink->SetEnabledArea(NavArea);
	}
	NavLink->SetLinkData(FVector(-NavLinkHalfLength, 0.0, 0.0), FVector(NavLinkHalfLength, 0.0, 0.0), ENavLinkDirection::BothWays);
}

void AHollowDoor::BeginPlay()
{
	Super::BeginPlay();
	LeafClosedLocation = Leaf->GetRelativeLocation();
	LeafMaterial = Leaf->CreateDynamicMaterialInstance(0);
	StateChangedHandle = DoorState->OnStateChanged.AddUObject(this, &ThisClass::HandleStateChanged);
	UpdateLeaf();
	UpdateWeldColor();
}

void AHollowDoor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DoorState->OnStateChanged.Remove(StateChangedHandle);
	GetWorld()->GetTimerManager().ClearTimer(LeafTimer);
	Super::EndPlay(EndPlayReason);
}

bool AHollowDoor::IsDoorwayClear() const
{
	// Asked when it matters (closing on the server), so it never depends on overlap bookkeeping.
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowDoorway), /*bTraceComplex=*/false, this);
	return !GetWorld()->OverlapAnyTestByObjectType(Doorway->GetComponentLocation(), Doorway->GetComponentQuat(),
		FCollisionObjectQueryParams(ECC_Pawn), Doorway->GetCollisionShape(), Params);
}

bool AHollowDoor::CanAcceptToolWork(const FHollowToolWork& Work) const
{
	const TOptional<EHollowDoorEvent> Event = DoorState->ToolEvent(Work.ToolTag);
	return Event.IsSet() && DoorState->Accepts(Event.GetValue());
}

void AHollowDoor::ApplyToolWork(const FHollowToolWork& Work)
{
	if (const TOptional<EHollowDoorEvent> Event = DoorState->ToolEvent(Work.ToolTag))
	{
		DoorState->ApplyTool(Event.GetValue(), Work.Amount);
	}
}

bool AHollowDoor::GetTargetStatus(const FGameplayTag& ToolTag, FHollowTargetStatus& OutStatus) const
{
	const UHollowDoorDefinition* Definition = DoorState->GetDefinition();
	if (!Definition || !DoorState->ToolEvent(ToolTag).IsSet())
	{
		return false;   // not a tool that works on doors
	}
	OutStatus.Value = DoorState->GetWeld();
	OutStatus.Marker = Definition->SealThreshold;
	return true;
}

FHollowInteractionInfo AHollowDoor::GetInteractionInfo(const AActor* Interactor) const
{
	FHollowInteractionInfo Info;
	if (const UHollowDoorDefinition* Definition = DoorState->GetDefinition())
	{
		Info.Prompt = DoorState->GetNetState().State == EHollowDoorState::Open ? Definition->PromptClose : Definition->PromptOpen;
	}
	return Info;
}

bool AHollowDoor::CanInteract(const AActor* Interactor) const
{
	// The doorway test belongs to the server's decision; the prompt shows whenever the state allows it.
	return DoorState->Accepts(EHollowDoorEvent::Interact);
}

void AHollowDoor::Interact(AActor* Interactor)
{
	DoorState->Interact(IsDoorwayClear());
}

void AHollowDoor::HandleStateChanged(const FHollowDoorNetState& Old, const FHollowDoorNetState& New, const bool bFresh)
{
	if (HasAuthority())
	{
		UpdateNavArea();   // the navmesh lives on the server only
	}
	UpdateWeldColor();
	UpdateLeaf();
	const bool bMoving = New.State == EHollowDoorState::Opening || New.State == EHollowDoorState::Closing;
	if (bMoving && LeafUpdateRateHz > 0.f && !GetWorld()->GetTimerManager().IsTimerActive(LeafTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(LeafTimer, this, &ThisClass::UpdateLeaf, 1.f / LeafUpdateRateHz, /*bLoop=*/true);
	}
}

void AHollowDoor::UpdateLeaf()
{
	const UHollowDoorDefinition* Definition = DoorState->GetDefinition();
	if (!Definition)
	{
		return;
	}
	const FHollowDoorNetState& State = DoorState->GetNetState();
	ShownAlpha = HollowDoorRules::LeafAlpha(State, DoorState->GetServerTime(), *Definition);
	Leaf->SetRelativeLocation(LeafClosedLocation + LeafOpenOffset * ShownAlpha);
	Leaf->SetCollisionResponseToChannel(ECC_Pawn, HollowDoorRules::IsLeafBlocking(State.State, ShownAlpha, *Definition) ? ECR_Block : ECR_Ignore);

	// The leaf settles when the motion ends; the server's state change follows a moment later.
	const bool bSettled = (State.State == EHollowDoorState::Opening && ShownAlpha >= 1.f) || (State.State == EHollowDoorState::Closing && ShownAlpha <= 0.f)
		|| (State.State != EHollowDoorState::Opening && State.State != EHollowDoorState::Closing);
	if (bSettled)
	{
		GetWorld()->GetTimerManager().ClearTimer(LeafTimer);
	}
}

void AHollowDoor::UpdateNavArea()
{
	const UHollowDoorDefinition* Definition = DoorState->GetDefinition();
	const TSubclassOf<UNavArea> NewArea = Definition ? HollowDoorNav::AreaFor(DoorState->GetNetState(), DoorState->GetWeld(), *Definition) : nullptr;
	if (!NewArea || NewArea == NavArea)
	{
		return;
	}
	const TSubclassOf<UNavArea> OldArea = NavArea;
	NavArea = NewArea;
	NavLink->SetEnabledArea(NewArea);   // patches the link in built tiles: O(1), no rebuild (ADR-0004)
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		// SetEnabledArea leaves the octree's copy of the link alone; without this a later rebuild of a neighbouring tile
		// (breach, nav modifier) would bring the old area back. Changes cached data only, dirties nothing.
		NavSys->K2_ReplaceAreaInOctreeData(NavLink, OldArea, NewArea);
	}
	if (UHollowNavUpdateSubsystem* Navigation = GetWorld()->GetSubsystem<UHollowNavUpdateSubsystem>())
	{
		Navigation->NotifyLinkChanged(this, NavLink->GetId(), OldArea, NewArea);
	}
}

void AHollowDoor::UpdateWeldColor()
{
	if (LeafMaterial && !WeldColorParameter.IsNone())
	{
		LeafMaterial->SetVectorParameterValue(WeldColorParameter, FLinearColor::LerpUsingHSV(ClosedColor, WeldedColor, DoorState->GetWeld()));
	}
}
