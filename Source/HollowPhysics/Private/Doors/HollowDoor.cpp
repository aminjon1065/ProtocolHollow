#include "Doors/HollowDoor.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Collision/HollowCollisionChannels.h"
#include "Destruction/HollowDestructionBudgetSubsystem.h"
#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HollowPhysicsLog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Nav/HollowDoorNav.h"
#include "Nav/HollowNavUpdateSubsystem.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "NavAreas/NavArea_Null.h"
#include "NavLinkCustomComponent.h"
#include "NavigationSystem.h"
#include "Tags/HollowGameplayTags.h"
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
	LeafCollision = Leaf->GetCollisionEnabled();
	LeafMaterial = Leaf->CreateDynamicMaterialInstance(0);
	StateChangedHandle = DoorState->OnStateChanged.AddUObject(this, &ThisClass::HandleStateChanged);
	UpdateLeaf();
	UpdateWeldColor();
}

void AHollowDoor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndTornLeaf();
	DoorState->OnStateChanged.Remove(StateChangedHandle);
	// Also reached from garbage collection of a torn-down world (a component's BeginDestroy ends play): the world may be gone.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LeafTimer);
	}
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

bool AHollowDoor::CanTakeStructuralDamage() const
{
	return DoorState->Accepts(EHollowDoorEvent::Structural);
}

bool AHollowDoor::ApplyStructuralDamage(const FHollowStructuralHit& Hit)
{
	FVector Direction = Hit.Direction;
	if (Direction.IsNearlyZero() && Hit.Instigator.IsValid())
	{
		Direction = GetActorLocation() - Hit.Instigator->GetActorLocation();
	}
	const bool bBackwards = FVector::DotProduct(Direction, GetActorForwardVector()) < 0.0;
	const EHollowDoorOutcome Outcome = DoorState->ApplyStructural(Hit.Amount, bBackwards);
	const bool bChanged = Outcome == EHollowDoorOutcome::StateChanged || Outcome == EHollowDoorOutcome::StructureChanged;
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this); Noise && bChanged)
	{
		// The hitter made the noise (ADR-0018): a dent is a heavy impact, the last blow a breach.
		const bool bBreached = DoorState->GetNetState().State == EHollowDoorState::Breached;
		Noise->ReportNoise(FHollowNoiseEvent(bBreached ? HollowTags::Noise_Structural_Breach : HollowTags::Noise_Impact_Heavy, GetActorLocation(), Hit.Instigator.Get()));
	}
	return bChanged;
}

EHollowPassage AHollowDoor::GetPassage() const
{
	const UHollowDoorDefinition* Definition = DoorState->GetDefinition();
	const FHollowDoorNetState& State = DoorState->GetNetState();
	switch (State.State)
	{
	case EHollowDoorState::Open:
	case EHollowDoorState::Breached:
		return EHollowPassage::Clear;
	case EHollowDoorState::Opening:
		// Passable once the leaf has travelled far enough, from server time like everything about the leaf.
		return Definition && !HollowDoorRules::IsLeafBlocking(State.State, HollowDoorRules::LeafAlpha(State, DoorState->GetServerTime(), *Definition), *Definition)
			? EHollowPassage::Clear : EHollowPassage::Open;
	case EHollowDoorState::Closing:
		return EHollowPassage::Open;   // wait for it to close, then open it
	case EHollowDoorState::Closed:
		return State.HasFlag(EHollowDoorFlags::Locked) || State.HasFlag(EHollowDoorFlags::Barricaded) ? EHollowPassage::Breach : EHollowPassage::Open;
	case EHollowDoorState::Welded:
		return EHollowPassage::Breach;
	}
	return EHollowPassage::Breach;
}

void AHollowDoor::HandleStateChanged(const FHollowDoorNetState& Old, const FHollowDoorNetState& New, const bool bFresh)
{
	if (HasAuthority())
	{
		UpdateNavArea();   // the navmesh lives on the server only
	}
	if (New.State == EHollowDoorState::Breached && Old.State != EHollowDoorState::Breached && bFresh)
	{
		TearLeafOff(New.HasFlag(EHollowDoorFlags::BreachedBackwards));
	}
	PlayStateSounds(Old, New, bFresh);
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
	// A breached door has no leaf anywhere (late joiners included): nothing to see, hit or trace against.
	const bool bHasLeaf = State.State != EHollowDoorState::Breached;
	Leaf->SetVisibility(bHasLeaf);
	Leaf->SetCollisionEnabled(bHasLeaf ? LeafCollision.GetValue() : ECollisionEnabled::NoCollision);

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
		const FLinearColor Welded = FLinearColor::LerpUsingHSV(ClosedColor, WeldedColor, DoorState->GetWeld());
		LeafMaterial->SetVectorParameterValue(WeldColorParameter, FLinearColor::LerpUsingHSV(Welded, DamagedColor, 1.f - DoorState->GetStructure()));
	}
}

void AHollowDoor::TearLeafOff(const bool bBackwards)
{
	// Cosmetic and local (ADR-0002): a copy of the leaf flies off along the blow; the real leaf is already gone.
	UHollowDestructionBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UHollowDestructionBudgetSubsystem>();
	const UHollowDoorDefinition* Definition = DoorState->GetDefinition();
	if (!Budget || !Definition || !Leaf->GetStaticMesh() || TornLeaf)
	{
		return;
	}
	TornLeafHandle = Budget->BeginLive(/*Bodies=*/1, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTornLeafExpired));
	if (TornLeafHandle == INDEX_NONE)
	{
		return;   // over budget: the leaf just disappears
	}
	TornLeaf = NewObject<UStaticMeshComponent>(this, TEXT("TornLeaf"), RF_Transient);
	TornLeaf->SetMobility(EComponentMobility::Movable);   // a static component refuses a mesh in a begun world
	TornLeaf->SetStaticMesh(Leaf->GetStaticMesh());
	for (int32 Index = 0; Index < Leaf->GetNumMaterials(); ++Index)
	{
		TornLeaf->SetMaterial(Index, Leaf->GetMaterial(Index));
	}
	TornLeaf->SetCanEverAffectNavigation(false);
	TornLeaf->SetCollisionProfileName(HollowCollision::DebrisProfile());
	TornLeaf->SetWorldTransform(Leaf->GetComponentTransform());   // called before the leaf is hidden: it is still where it stood
	TornLeaf->RegisterComponent();
	TornLeaf->SetSimulatePhysics(true);
	TornLeaf->SetPhysicsLinearVelocity(GetActorForwardVector() * (bBackwards ? -Definition->TornLeafSpeed : Definition->TornLeafSpeed));
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: leaf torn off %s (cosmetic)"), *GetName(), bBackwards ? TEXT("backwards") : TEXT("forwards"));
}

void AHollowDoor::HandleTornLeafExpired()
{
	EndTornLeaf();
}

void AHollowDoor::EndTornLeaf()
{
	if (TornLeaf)
	{
		TornLeaf->DestroyComponent();
		TornLeaf = nullptr;
	}
	if (TornLeafHandle != INDEX_NONE)
	{
		const UWorld* World = GetWorld();
		if (UHollowDestructionBudgetSubsystem* Budget = World ? World->GetSubsystem<UHollowDestructionBudgetSubsystem>() : nullptr)
		{
			Budget->EndLive(TornLeafHandle);
		}
		TornLeafHandle = INDEX_NONE;
	}
}

void AHollowDoor::PlayStateSounds(const FHollowDoorNetState& Old, const FHollowDoorNetState& New, const bool bFresh) const
{
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	if (!Audio)
	{
		return;
	}
	if (New.State != Old.State)
	{
		if (!bFresh)
		{
			return;   // an old transition (a late joiner): nothing to hear
		}
		if (New.State == EHollowDoorState::Opening || New.State == EHollowDoorState::Closing)
		{
			Audio->Play(HollowTags::Sound_Door_Servo, GetActorLocation());
		}
		else if (New.State == EHollowDoorState::Breached)
		{
			Audio->Play(HollowTags::Noise_Structural_Breach, GetActorLocation());   // what the AI heard on the server
		}
	}
	else if (New.StructureQ < Old.StructureQ)
	{
		// A blow is sent as it lands (no transition, so never "fresh"); a door hears none before its BeginPlay.
		Audio->Play(HollowTags::Noise_Impact_Heavy, GetActorLocation());
	}
}
