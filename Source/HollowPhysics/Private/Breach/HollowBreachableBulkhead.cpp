#include "Breach/HollowBreachableBulkhead.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Breach/HollowBreachableDefinition.h"
#include "Breach/HollowStructuralIntegrityComponent.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Destruction/HollowDestructionBudgetSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "HollowPhysicsLog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Nav/HollowNavUpdateSubsystem.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "NavAreas/NavArea_Null.h"
#include "NavLinkCustomComponent.h"
#include "NavigationSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowBreachableBulkhead)

AHollowBreachableBulkhead::AHollowBreachableBulkhead(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	NetDormancy = DORM_DormantAll;   // server-spawned: sent once, then only on FlushNetDormancy (ADR-0002)

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(Root);
	Frame->SetCanEverAffectNavigation(false);   // the bulkhead reaches navigation through NavBlocker and NavLink only

	Integrity = CreateDefaultSubobject<UHollowStructuralIntegrityComponent>(TEXT("Integrity"));

	NavBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("NavBlocker"));
	NavBlocker->SetupAttachment(Root);
	NavBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavBlocker->SetCanEverAffectNavigation(true);
	NavBlocker->SetCustomNavigableGeometry(EHasCustomNavigableGeometry::EvenIfNotCollidable);   // relevant without blocking anyone
	NavBlocker->bDynamicObstacle = true;
	NavBlocker->SetAreaClassOverride(UNavArea_Null::StaticClass());

	NavLink = CreateDefaultSubobject<UNavLinkCustomComponent>(TEXT("NavLink"));
}

void AHollowBreachableBulkhead::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Zone panels and their rubble: the same components on every machine, from the same definition.
	const UHollowBreachableDefinition* Definition = Integrity->GetDefinition();
	const int32 Zones = Definition ? FMath::Min(Definition->Zones.Num(), FHollowBreachNetState::MaxZones) : 0;
	const FVector PanelUnit = MeshHalfSize(PanelMesh);
	const FVector RubbleUnit = MeshHalfSize(RubbleMesh);
	for (int32 Index = 0; Index < Zones; ++Index)
	{
		const FHollowBreachZone& Zone = Definition->Zones[Index];
		UStaticMeshComponent* Panel = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Panel%d"), Index));
		Panel->SetupAttachment(Root);
		Panel->SetCanEverAffectNavigation(false);
		Panel->SetStaticMesh(PanelMesh);
		Panel->SetMaterial(0, PanelMaterial);
		Panel->SetRelativeLocation(Zone.Center);
		Panel->SetRelativeScale3D(Zone.HalfSize / PanelUnit);
		Panel->RegisterComponent();
		Panels.Add(Panel);

		UStaticMeshComponent* Pile = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Rubble%d"), Index));
		Pile->SetupAttachment(Root);
		Pile->SetCanEverAffectNavigation(false);
		Pile->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // cosmetic: pawns walk through, nothing rests on it
		Pile->SetStaticMesh(RubbleMesh);
		Pile->SetMaterial(0, RubbleMaterial);
		Pile->SetRelativeLocation(FVector(0.0, Zone.Center.Y, Definition->RubbleHeight * 0.5));
		Pile->SetRelativeScale3D(FVector(Definition->RubbleDepth * 0.5, Zone.HalfSize.Y, Definition->RubbleHeight * 0.5) / RubbleUnit);
		Pile->SetVisibility(false);
		Pile->RegisterComponent();
		Rubble.Add(Pile);
	}
	Presented.SetNum(Zones);
	LiveHandles.Init(INDEX_NONE, Zones);
	LiveFractures.SetNum(Zones);

	// Before the sector's navmesh is built (the server spawns bulkheads first), so the link is part of it from the
	// start. Area first: SetLinkData refreshes the link's navigation data, so the octree copy starts with this area.
	if (Definition)
	{
		NavArea = Definition->AreaIntact;
		NavLink->SetEnabledArea(NavArea);
	}
	NavLink->SetLinkData(FVector(-NavLinkHalfLength, 0.0, 0.0), FVector(NavLinkHalfLength, 0.0, 0.0), ENavLinkDirection::BothWays);
}

void AHollowBreachableBulkhead::BeginPlay()
{
	Super::BeginPlay();
	for (UStaticMeshComponent* Panel : Panels)
	{
		PanelMaterials.Add(Panel->CreateDynamicMaterialInstance(0));
	}
	StateChangedHandle = Integrity->OnStateChanged.AddUObject(this, &ThisClass::HandleStateChanged);
	RequestFractureAssets();
	// A late joiner (or regained relevancy) starts here with zones already broken: the policy snaps them (age, unseen).
	PresentNewBreaks(Integrity->GetNetState());
	UpdateDamageColor();
}

void AHollowBreachableBulkhead::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Integrity->OnStateChanged.Remove(StateChangedHandle);
	// Also reached from garbage collection of a torn-down world (a component's BeginDestroy ends play): the world may be gone.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BlockerTimer);
	}
	for (int32 Zone = 0; Zone < LiveHandles.Num(); ++Zone)
	{
		EndLiveFracture(Zone);
	}
	if (FractureHandle.IsValid())
	{
		FractureHandle->CancelHandle();
		FractureHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

bool AHollowBreachableBulkhead::IsNavBlockerActive() const
{
	return NavBlocker->CanEverAffectNavigation();
}

UStaticMeshComponent* AHollowBreachableBulkhead::GetPanel(const int32 Zone) const
{
	return Panels.IsValidIndex(Zone) ? Panels[Zone].Get() : nullptr;
}

UStaticMeshComponent* AHollowBreachableBulkhead::GetRubble(const int32 Zone) const
{
	return Rubble.IsValidIndex(Zone) ? Rubble[Zone].Get() : nullptr;
}

UGeometryCollectionComponent* AHollowBreachableBulkhead::GetLiveFracture(const int32 Zone) const
{
	return LiveFractures.IsValidIndex(Zone) ? LiveFractures[Zone].Get() : nullptr;
}

TOptional<EHollowBreachPresentation> AHollowBreachableBulkhead::GetZonePresentation(const int32 Zone) const
{
	return Presented.IsValidIndex(Zone) ? Presented[Zone] : TOptional<EHollowBreachPresentation>();
}

FVector AHollowBreachableBulkhead::GetZoneCenter(const int32 Zone) const
{
	const UStaticMeshComponent* Panel = GetPanel(Zone);
	return Panel ? Panel->GetComponentLocation() : GetActorLocation();
}

bool AHollowBreachableBulkhead::CanTakeStructuralDamage() const
{
	return Integrity->CanTakeDamage();
}

EHollowPassage AHollowBreachableBulkhead::GetPassage() const
{
	return Integrity->GetNetState().State == EHollowBreachState::Breached ? EHollowPassage::Clear : EHollowPassage::Breach;
}

bool AHollowBreachableBulkhead::ApplyStructuralDamage(const FHollowStructuralHit& Hit)
{
	FVector Direction = Hit.Direction;
	if (Direction.IsNearlyZero() && Hit.Instigator.IsValid())
	{
		Direction = GetActorLocation() - Hit.Instigator->GetActorLocation();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector();
	}
	const EHollowBreachOutcome Outcome = Integrity->ApplyHit(Hit.Location, Hit.Amount, Direction.GetSafeNormal());
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this); Noise && Outcome != EHollowBreachOutcome::Rejected)
	{
		// The hitter made the noise (ADR-0018): a broken zone is a breach, any other blow a heavy impact.
		const FGameplayTag Tag = Outcome == EHollowBreachOutcome::ZoneBroken ? HollowTags::Noise_Structural_Breach : HollowTags::Noise_Impact_Heavy;
		Noise->ReportNoise(FHollowNoiseEvent(Tag, Hit.Location, Hit.Instigator.Get()));
	}
	return Outcome != EHollowBreachOutcome::Rejected;
}

void AHollowBreachableBulkhead::HandleStateChanged(const FHollowBreachNetState& Old, const FHollowBreachNetState& New)
{
	if (HasAuthority() && Old.State != EHollowBreachState::Breached && New.State == EHollowBreachState::Breached)
	{
		OpenPassage();   // the navmesh lives on the server only
	}
	PlayStateSounds(Old, New);   // before the breaks are shown: it reads which zones are new here
	PresentNewBreaks(New);
	UpdateDamageColor();
}

void AHollowBreachableBulkhead::PlayStateSounds(const FHollowBreachNetState& Old, const FHollowBreachNetState& New) const
{
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	const UHollowBreachableDefinition* Definition = Integrity->GetDefinition();
	if (!Audio || !Definition)
	{
		return;
	}
	if (const uint8 NewZones = HollowBreachRules::NewlyBroken(ShownMask, New))
	{
		// Heard whether or not this machine can afford to show the fracture; an old break (a late joiner) is silent.
		if (Integrity->GetServerTime() - New.BreachServerTime <= Definition->FreshWindowSeconds)
		{
			Audio->Play(HollowTags::Noise_Structural_Breach, GetZoneCenter(FMath::CountTrailingZeros(static_cast<uint32>(NewZones))));
		}
	}
	else if (New.DamageStageQ > Old.DamageStageQ)
	{
		Audio->Play(HollowTags::Noise_Impact_Heavy, GetActorLocation());   // a dent, as the AI heard it
	}
}

void AHollowBreachableBulkhead::PresentNewBreaks(const FHollowBreachNetState& State)
{
	const uint8 NewZones = HollowBreachRules::NewlyBroken(ShownMask, State);
	for (int32 Zone = 0; Zone < Panels.Num(); ++Zone)
	{
		if ((NewZones & (1u << Zone)) != 0)
		{
			PresentZoneBreak(Zone, ChoosePresentation(Zone, State), State.GetImpactDirection());
		}
	}
}

EHollowBreachPresentation AHollowBreachableBulkhead::ChoosePresentation(const int32 Zone, const FHollowBreachNetState& State) const
{
	const UHollowBreachableDefinition* Definition = Integrity->GetDefinition();
	const UHollowDestructionBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UHollowDestructionBudgetSubsystem>();
	const UGeometryCollection* Collection = Definition ? Definition->Zones[Zone].Fracture.Get() : nullptr;
	if (!Budget || !Collection)
	{
		return EHollowBreachPresentation::Snap;   // nothing to simulate with on this machine
	}
	FHollowBreachPresentationInput Input;
	Input.AgeSeconds = Integrity->GetServerTime() - State.BreachServerTime;
	Input.FreshWindowSeconds = Definition->FreshWindowSeconds;
	if (const TOptional<FVector> View = Budget->GetViewLocation())
	{
		Input.ViewDistance = FVector::Dist(View.GetValue(), GetZoneCenter(Zone));
	}
	Input.bRecentlyRendered = Panels[Zone]->WasRecentlyRendered(GetDefault<UHollowDestructionBudgetConfig>()->RecentlyRenderedSeconds);
	Input.NewBodies = CountFractureBodies(Collection);
	const EHollowBreachPresentation Presentation = Budget->Choose(Input);
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: zone %d broke %.2f s ago, camera %s, seen %d, %d live / %d bodies + %d -> %s"), *GetName(), Zone, Input.AgeSeconds,
		Input.ViewDistance ? *FString::Printf(TEXT("%.0f cm"), Input.ViewDistance.GetValue()) : TEXT("none"), Input.bRecentlyRendered,
		Budget->GetLiveBreaches(), Budget->GetLiveBodies(), Input.NewBodies, *HollowEnumToString(Presentation));
	return Presentation;
}

void AHollowBreachableBulkhead::PresentZoneBreak(const int32 Zone, EHollowBreachPresentation Presentation, const FVector& ImpactDirection)
{
	if (!Panels.IsValidIndex(Zone) || (ShownMask & (1u << Zone)) != 0)
	{
		return;
	}
	ShownMask |= static_cast<uint8>(1u << Zone);

	// Gameplay part, the same everywhere: the panel no longer blocks.
	UStaticMeshComponent* Panel = Panels[Zone];
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Panel->SetVisibility(false);

	const UHollowBreachableDefinition* Definition = Integrity->GetDefinition();
	UGeometryCollection* Collection = Definition ? Definition->Zones[Zone].Fracture.Get() : nullptr;
	UHollowDestructionBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UHollowDestructionBudgetSubsystem>();
	if (Presentation == EHollowBreachPresentation::Full && Collection && Budget)
	{
		LiveHandles[Zone] = Budget->BeginLive(CountFractureBodies(Collection), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleLiveExpired, Zone));
	}
	if (LiveHandles[Zone] == INDEX_NONE)
	{
		Presentation = EHollowBreachPresentation::Snap;
	}
	Presented[Zone] = Presentation;
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: zone %d presented %s"), *GetName(), Zone, *HollowEnumToString(Presentation));
	if (Presentation == EHollowBreachPresentation::Snap)
	{
		ShowRubble(Zone);
		return;
	}

	// Cosmetic part, local to this machine (ADR-0002): never replicated, never touches pawns or navigation.
	UGeometryCollectionComponent* Fracture = NewObject<UGeometryCollectionComponent>(this, *FString::Printf(TEXT("Fracture%d"), Zone));
	Fracture->SetupAttachment(Root);
	Fracture->SetRelativeLocation(Definition->Zones[Zone].Center);
	Fracture->SetCanEverAffectNavigation(false);
	Fracture->SetIsReplicated(false);
	Fracture->SetEnableReplication(false);
	Fracture->SetCollisionProfileName(HollowCollision::DebrisProfile());
	Fracture->SetRestCollection(Collection);
	Fracture->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	Fracture->InitialVelocityType = EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined;
	Fracture->InitialLinearVelocity = ImpactDirection.GetSafeNormal() * Definition->DebrisSpeed;
	Fracture->SetSimulatePhysics(true);
	if (UPhysicalMaterial* Material = Definition->DebrisMaterial.Get())
	{
		Fracture->SetPhysMaterialOverride(Material);
	}
	Fracture->RegisterComponent();
	LiveFractures[Zone] = Fracture;
}

void AHollowBreachableBulkhead::HandleLiveExpired(const int32 Zone)
{
	LiveHandles[Zone] = INDEX_NONE;   // the budget let it go already
	EndLiveFracture(Zone);
	ShowRubble(Zone);
}

void AHollowBreachableBulkhead::EndLiveFracture(const int32 Zone)
{
	if (LiveHandles.IsValidIndex(Zone) && LiveHandles[Zone] != INDEX_NONE)
	{
		if (UHollowDestructionBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UHollowDestructionBudgetSubsystem>())
		{
			Budget->EndLive(LiveHandles[Zone]);
		}
		LiveHandles[Zone] = INDEX_NONE;
	}
	if (LiveFractures.IsValidIndex(Zone) && LiveFractures[Zone])
	{
		LiveFractures[Zone]->DestroyComponent();
		LiveFractures[Zone] = nullptr;
	}
}

void AHollowBreachableBulkhead::ShowRubble(const int32 Zone)
{
	if (Rubble.IsValidIndex(Zone))
	{
		Rubble[Zone]->SetVisibility(true);
	}
}

void AHollowBreachableBulkhead::OpenPassage()
{
	const UHollowBreachableDefinition* Definition = Integrity->GetDefinition();
	const TSubclassOf<UNavArea> NewArea = Definition ? Definition->AreaBreached : nullptr;
	if (NewArea && NewArea != NavArea)
	{
		const TSubclassOf<UNavArea> OldArea = NavArea;
		NavArea = NewArea;
		NavLink->SetEnabledArea(NewArea);   // patches the link in built tiles: O(1), agents path through at once (ADR-0004)
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			// SetEnabledArea leaves the octree's copy of the link alone; the blocker's tile rebuild below would otherwise
			// bring the intact area back (S2-05).
			NavSys->K2_ReplaceAreaInOctreeData(NavLink, OldArea, NewArea);
		}
		if (UHollowNavUpdateSubsystem* Navigation = GetWorld()->GetSubsystem<UHollowNavUpdateSubsystem>())
		{
			Navigation->NotifyLinkChanged(this, NavLink->GetId(), OldArea, NewArea);
		}
	}
	const float Delay = Definition ? Definition->BlockerRemovalDelay : 0.f;
	if (Delay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(BlockerTimer, this, &ThisClass::RemoveNavBlocker, Delay, /*bLoop=*/false);
	}
	else
	{
		RemoveNavBlocker();
	}
}

void AHollowBreachableBulkhead::RemoveNavBlocker()
{
	// Leaves the nav octree: the navigation system rebuilds the tiles under its bounds (plus agent radius), async.
	NavBlocker->SetCanEverAffectNavigation(false);
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: nav blocker removed"), *GetName());
}

void AHollowBreachableBulkhead::UpdateDamageColor()
{
	const float Stage = Integrity->GetNetState().DamageStageQ / static_cast<float>(TNumericLimits<uint8>::Max());
	for (UMaterialInstanceDynamic* Material : PanelMaterials)
	{
		if (Material && !DamageColorParameter.IsNone())
		{
			Material->SetVectorParameterValue(DamageColorParameter, FLinearColor::LerpUsingHSV(IntactColor, DamagedColor, Stage));
		}
	}
}

void AHollowBreachableBulkhead::RequestFractureAssets()
{
	const UHollowBreachableDefinition* Definition = Integrity->GetDefinition();
	if (!Definition || !FApp::CanEverRender())
	{
		return;   // nobody watches here: every break snaps, no geometry collection is ever loaded
	}
	TArray<FSoftObjectPath> Paths;
	for (const FHollowBreachZone& Zone : Definition->Zones)
	{
		Paths.AddUnique(Zone.Fracture.ToSoftObjectPath());
	}
	Paths.AddUnique(Definition->DebrisMaterial.ToSoftObjectPath());
	Paths.RemoveAll([](const FSoftObjectPath& Path) { return Path.IsNull(); });
	FractureHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths));
}

int32 AHollowBreachableBulkhead::CountFractureBodies(const UGeometryCollection* Collection)
{
	const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Geometry = Collection ? Collection->GetGeometryCollection() : nullptr;
	if (!Geometry.IsValid())
	{
		return 0;
	}
	int32 Leaves = 0;
	for (int32 Transform = 0; Transform < Geometry->NumElements(FGeometryCollection::TransformGroup); ++Transform)
	{
		Leaves += Geometry->Children[Transform].IsEmpty() && Geometry->TransformToGeometryIndex[Transform] != INDEX_NONE ? 1 : 0;
	}
	return Leaves;
}

FVector AHollowBreachableBulkhead::MeshHalfSize(const UStaticMesh* Mesh)
{
	const FVector Extent = Mesh ? Mesh->GetBounds().BoxExtent : FVector::OneVector;
	return Extent.ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
}
