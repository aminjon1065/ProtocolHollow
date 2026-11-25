#include "World/HollowLootCrate.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Breach/HollowBreachableBulkhead.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Destruction/HollowDestructionBudgetSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "HollowGameLog.h"
#include "Items/HollowItemDefinition.h"
#include "Loot/HollowLootTable.h"
#include "Math/RandomStream.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "TimerManager.h"
#include "World/HollowLootPickup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLootCrate)

namespace HollowLootCratePrivate
{
	/** The broken crate goes dormant again this long after its break went out. */
	constexpr float DormantAgainSeconds = 2.f;
	/** Pickups start this far apart around the crate's middle (cm). */
	constexpr float SpillScatter = 15.f;
	/** How much each pickup flies out along its own offset from the middle, against the break's direction (1). */
	constexpr float SpillSpread = 0.6f;
}

AHollowLootCrate::AHollowLootCrate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	NetDormancy = DORM_Initial;   // nothing to say until it breaks
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Bounds->SetCanEverAffectNavigation(true);
	Bounds->bDynamicObstacle = true;   // the AI walks round it (a modifier: no navmesh rebuild, CLAUDE.md sec. 5)
	SetRootComponent(Bounds);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Bounds);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
}

void AHollowLootCrate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowLootCrate, NetState, Params);
}

void AHollowLootCrate::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	if (!Fracture.IsNull() && GetNetMode() != NM_DedicatedServer && !NetState.bBroken && UAssetManager::IsInitialized())
	{
		FractureLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Fracture.ToSoftObjectPath());
	}
	if (NetState.bBroken)
	{
		PresentBreak();   // spawned broken (a late join): no fracture, its age says so
	}
}

void AHollowLootCrate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndFracture();
	if (FractureLoad.IsValid())
	{
		FractureLoad->CancelHandle();
		FractureLoad.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void AHollowLootCrate::SetSpawnSeed(const int32 InSeed)
{
	Seed = InSeed;
	bRolled = false;
}

void AHollowLootCrate::EnsureContents() const
{
	if (!bRolled)
	{
		Contents = LootTable ? LootTable->Roll(Seed) : TArray<FHollowItemStack>();
		bRolled = true;
	}
}

TArray<FHollowItemStack> AHollowLootCrate::GetContents() const
{
	EnsureContents();
	return Contents;
}

bool AHollowLootCrate::ApplyPropHit(const FHollowStructuralHit& Hit)
{
	if (!HasAuthority() || NetState.bBroken || Hit.Amount <= 0.f)
	{
		return false;
	}
	Health = FMath::Max(Health - Hit.Amount, 0.f);
	UE_LOG(LogHollowGame, Verbose, TEXT("%s: hit for %.0f, %.0f left"), *GetName(), Hit.Amount, Health);
	if (Health <= 0.f)
	{
		const FVector Direction = !Hit.Direction.IsNearlyZero() ? Hit.Direction
			: Hit.Instigator.IsValid() ? (GetActorLocation() - Hit.Instigator->GetActorLocation()).GetSafeNormal() : FVector::ZeroVector;
		Break(Direction, Hit.Instigator.Get());
	}
	return true;
}

bool AHollowLootCrate::CanAcceptToolWork(const FHollowToolWork& Work) const
{
	return !NetState.bBroken && CutterTag.IsValid() && Work.ToolTag.MatchesTagExact(CutterTag);
}

void AHollowLootCrate::ApplyToolWork(const FHollowToolWork& Work)
{
	FHollowStructuralHit Hit;
	Hit.Amount = Work.Amount * CutDamagePerWork;
	Hit.Location = Work.ImpactPoint;
	Hit.Direction = -Work.ImpactNormal;
	Hit.Instigator = Work.Instigator;
	ApplyPropHit(Hit);
}

void AHollowLootCrate::Break(const FVector& Direction, AActor* Instigator)
{
	if (!HasAuthority() || NetState.bBroken)
	{
		return;
	}
	FlushNetDormancy();
	NetState.bBroken = true;
	NetState.BreakDirection = Direction.GetSafeNormal();
	NetState.BreakServerTime = static_cast<float>(GetServerTime());
	MARK_PROPERTY_DIRTY_FROM_NAME(AHollowLootCrate, NetState, this);
	ForceNetUpdate();
	Health = 0.f;
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this); Noise && BreakNoiseTag.IsValid())
	{
		Noise->ReportNoise(FHollowNoiseEvent(BreakNoiseTag, GetActorLocation(), Instigator));
	}
	SpillLoot(NetState.BreakDirection);
	UE_LOG(LogHollowGame, Log, TEXT("%s broken open by %s: %d stack(s) spill out"), *GetName(), *GetNameSafe(Instigator), Spilled.Num());
	PresentBreak();
	FTimerHandle Dormant;
	GetWorldTimerManager().SetTimer(Dormant, FTimerDelegate::CreateWeakLambda(this, [this] { SetNetDormancy(DORM_DormantAll); }),
		HollowLootCratePrivate::DormantAgainSeconds, /*bLoop=*/false);
}

void AHollowLootCrate::SpillLoot(const FVector& Direction)
{
	EnsureContents();
	UWorld* World = GetWorld();
	if (!PickupClass || !World || !UAssetManager::IsInitialized())
	{
		return;
	}
	FRandomStream Scatter(Seed);
	const FVector Middle = Bounds->GetComponentLocation();
	for (const FHollowItemStack& Stack : Contents)
	{
		UHollowItemDefinition* Item = Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Stack.Item).TryLoad());
		if (!Item || Stack.Count <= 0)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("%s: unknown loot %s"), *GetName(), *Stack.Item.ToString());
			continue;
		}
		const FVector Offset = FVector(Scatter.FRandRange(-1.f, 1.f), Scatter.FRandRange(-1.f, 1.f), 0.f) * HollowLootCratePrivate::SpillScatter;
		const FTransform Transform(FRotator(0.f, Scatter.FRandRange(0.f, 360.f), 0.f), Middle + Offset);
		AHollowLootPickup* Pickup = World->SpawnActorDeferred<AHollowLootPickup>(PickupClass, Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Pickup)
		{
			continue;
		}
		Pickup->SetLoot(Item, Stack.Count);
		Pickup->FinishSpawning(Transform);
		const FVector Away = (Direction + Offset.GetSafeNormal() * HollowLootCratePrivate::SpillSpread + FVector::UpVector).GetSafeNormal();
		Pickup->Launch(Away * SpillSpeed);
		Spilled.Add(Pickup);
	}
}

void AHollowLootCrate::OnRep_NetState()
{
	if (NetState.bBroken && HasActorBegunPlay())
	{
		PresentBreak();
	}
}

double AHollowLootCrate::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

void AHollowLootCrate::PresentBreak()
{
	if (bPresented)
	{
		return;
	}
	bPresented = true;
	// Gameplay part, the same everywhere: the crate no longer blocks or shapes navigation.
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetCanEverAffectNavigation(false);
	Body->SetVisibility(false);

	const double Age = FMath::Max(GetServerTime() - NetState.BreakServerTime, 0.0);
	if (Age > FreshWindowSeconds)
	{
		return;   // an old break (late join, or came into relevancy): just gone
	}
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this); Audio && BreakNoiseTag.IsValid())
	{
		Audio->Play(BreakNoiseTag, GetActorLocation());
	}
	UGeometryCollection* Collection = Fracture.Get();
	UHollowDestructionBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UHollowDestructionBudgetSubsystem>();
	if (!Collection || !Budget)
	{
		return;
	}
	FHollowBreachPresentationInput Input;
	Input.AgeSeconds = Age;
	Input.FreshWindowSeconds = FreshWindowSeconds;
	if (const TOptional<FVector> View = Budget->GetViewLocation())
	{
		Input.ViewDistance = FVector::Dist(View.GetValue(), GetActorLocation());
	}
	Input.bRecentlyRendered = Body->WasRecentlyRendered(GetDefault<UHollowDestructionBudgetConfig>()->RecentlyRenderedSeconds);
	Input.NewBodies = AHollowBreachableBulkhead::CountFractureBodies(Collection);
	if (Budget->Choose(Input) != EHollowBreachPresentation::Full)
	{
		return;
	}
	LiveHandle = Budget->BeginLive(Input.NewBodies, FSimpleDelegate::CreateWeakLambda(this, [this]
	{
		LiveHandle = INDEX_NONE;   // the budget let it go already
		EndFracture();
	}));
	if (LiveHandle == INDEX_NONE)
	{
		return;
	}
	// Cosmetic, local to this machine (ADR-0002): never replicated, never touches pawns or navigation.
	LiveFracture = NewObject<UGeometryCollectionComponent>(this, TEXT("Fracture"));
	LiveFracture->SetupAttachment(Body);
	LiveFracture->SetUsingAbsoluteScale(true);
	LiveFracture->SetCanEverAffectNavigation(false);
	LiveFracture->SetIsReplicated(false);
	LiveFracture->SetEnableReplication(false);
	LiveFracture->SetCollisionProfileName(HollowCollision::DebrisProfile());
	LiveFracture->SetRestCollection(Collection);
	LiveFracture->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	LiveFracture->InitialVelocityType = EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined;
	LiveFracture->InitialLinearVelocity = FVector(NetState.BreakDirection) * DebrisSpeed;
	LiveFracture->SetSimulatePhysics(true);
	LiveFracture->RegisterComponent();
	LiveFracture->SetWorldLocation(Bounds->GetComponentLocation());
}

void AHollowLootCrate::EndFracture()
{
	if (LiveHandle != INDEX_NONE)
	{
		if (UHollowDestructionBudgetSubsystem* Budget = GetWorld() ? GetWorld()->GetSubsystem<UHollowDestructionBudgetSubsystem>() : nullptr)
		{
			Budget->EndLive(LiveHandle);
		}
		LiveHandle = INDEX_NONE;
	}
	if (LiveFracture)
	{
		LiveFracture->DestroyComponent();
		LiveFracture = nullptr;
	}
}
