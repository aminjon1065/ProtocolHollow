#include "World/HollowArcHazard.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Attributes/HollowHealthSet.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "HollowGameLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Tags/HollowGameplayTags.h"
#include "Threat/HollowAIPlayers.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"
#include "World/HollowArcHazardDefinition.h"
#include "World/HollowSwitch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowArcHazard)

namespace HollowArcHazardPrivate
{
	/** A phase timer lands this long after the boundary, never before it (float timers). */
	constexpr float PastBoundarySeconds = 0.01f;
}

AHollowArcHazard::AHollowArcHazard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);   // it spans the passage the sector put it in
	NetDormancy = DORM_DormantAll;   // only the breaker changes it (ADR-0002)

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Emitters = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Emitters"));
	Emitters->SetupAttachment(Root);
	Emitters->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);   // on the walls; never in the way
	Emitters->SetCanEverAffectNavigation(false);

	Beams = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Beams"));
	Beams->SetupAttachment(Root);
	Beams->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Beams->SetCanEverAffectNavigation(false);   // the arcs hurt; they do not block (ADR-0039)
	Beams->SetCastShadow(false);
	Beams->SetVisibility(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);
}

void AHollowArcHazard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowArcHazard, ArcState, Params);
}

void AHollowArcHazard::BeginPlay()
{
	Super::BeginPlay();
	if (!Definition)
	{
		UE_LOG(LogHollowGame, Error, TEXT("%s has no arc definition."), *GetName());
		return;
	}
	for (const FTransform& Emitter : Definition->Emitters)
	{
		Emitters->AddInstance(Emitter);
	}
	for (const FTransform& Beam : Definition->Beams)
	{
		Beams->AddInstance(Beam);
	}
	GlowIntensity = Glow->Intensity;
	if (HasAuthority())
	{
		UWorld* World = GetWorld();
		if (Definition->BreakerClass && World)
		{
			FActorSpawnParameters Spawn;
			Spawn.Owner = this;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Breaker = World->SpawnActor<AHollowSwitch>(Definition->BreakerClass, Definition->BreakerOffset * GetActorTransform(), Spawn);
		}
		if (Breaker)
		{
			Breaker->OnToggled.AddUObject(this, &ThisClass::HandleBreakerToggled);
		}
		UE_LOG(LogHollowGame, Log, TEXT("%s spans the passage at %s, breaker %s"), *GetName(), *GetActorLocation().ToCompactString(), *GetNameSafe(Breaker));
		SetPowered(!Breaker || Breaker->IsOn());   // no breaker: always on
		return;
	}
	ApplyPhase();   // a state replicated before BeginPlay (a late joiner)
}

void AHollowArcHazard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())   // null when the garbage collector ends play
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (HasAuthority() && IsValid(Breaker))
	{
		Breaker->Destroy();   // the breaker is the arc's: it goes with it
	}
	Super::EndPlay(EndPlayReason);
}

double AHollowArcHazard::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

FHollowArcMoment AHollowArcHazard::GetMoment() const
{
	return Definition ? HollowArcRules::MomentAt(ServerNow() - ArcState.CycleStartServerTime, Definition->GetTimings(), ArcState.bPowered)
		: FHollowArcMoment();
}

bool AHollowArcHazard::Contains(const FVector& Point) const
{
	if (!Definition)
	{
		return false;
	}
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Point);
	const FVector& Extent = Definition->ZoneExtent;
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && Local.Z >= 0.0 && Local.Z <= 2.0 * Extent.Z;
}

void AHollowArcHazard::SetPowered(const bool bInPowered)
{
	if (!HasAuthority() || !Definition)
	{
		return;
	}
	ArcState.bPowered = bInPowered;
	ArcState.CycleStartServerTime = ServerNow();   // on: the warning first; off: nothing
	StruckCycle = INDEX_NONE;
	FlushNetDormancy();
	MARK_PROPERTY_DIRTY_FROM_NAME(AHollowArcHazard, ArcState, this);
	ForceNetUpdate();
	UE_LOG(LogHollowGame, Log, TEXT("%s: power %s"), *GetName(), bInPowered ? TEXT("on, warning first") : TEXT("off"));
	ApplyPhase();
}

void AHollowArcHazard::HandleBreakerToggled(const bool bIsOn)
{
	SetPowered(bIsOn);
}

void AHollowArcHazard::OnRep_ArcState()
{
	UE_LOG(LogHollowGame, Log, TEXT("%s: power %s (replicated)"), *GetName(), ArcState.bPowered ? TEXT("on") : TEXT("off"));
	if (HasActorBegunPlay())
	{
		ApplyPhase();
	}
}

void AHollowArcHazard::ApplyPhase()
{
	using namespace HollowArcHazardPrivate;
	if (!Definition)
	{
		return;
	}
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(PhaseTimer);
	Timers.ClearTimer(CrackleTimer);
	const FHollowArcMoment Moment = GetMoment();
	const bool bChanged = Moment.Phase != ShownPhase;
	ShownPhase = Moment.Phase;

	Beams->SetVisibility(Moment.Phase == EHollowArcPhase::Discharge);
	switch (Moment.Phase)
	{
	case EHollowArcPhase::Telegraph:
		bCrackleLit = false;
		Crackle();
		break;
	case EHollowArcPhase::Discharge:
		Glow->SetIntensity(GlowIntensity);
		Glow->SetVisibility(true);
		break;
	case EHollowArcPhase::Idle:
	case EHollowArcPhase::Off:
		Glow->SetVisibility(false);
		break;
	}
	UE_CLOG(bChanged, LogHollowGame, Verbose, TEXT("%s: %s"), *GetName(), *HollowEnumToString(Moment.Phase));
	// A fresh warning or strike is heard on every machine (from the replicated cycle, no RPC; S7-03).
	const FGameplayTag Sound = Moment.Phase == EHollowArcPhase::Telegraph ? Definition->TelegraphSound
		: Moment.Phase == EHollowArcPhase::Discharge ? Definition->DischargeSound : FGameplayTag();
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	if (bChanged && bPresented && Sound.IsValid() && Audio)
	{
		Audio->Play(Sound, Glow->GetComponentLocation());
	}
	bPresented = true;

	if (HasAuthority() && Moment.Phase == EHollowArcPhase::Discharge)
	{
		// One strike a cycle, as it starts: whoever stood in the warning and did not step out.
		const int64 Cycle = static_cast<int64>((ServerNow() - ArcState.CycleStartServerTime) / Definition->GetTimings().Period());
		if (Cycle != StruckCycle)
		{
			StruckCycle = Cycle;
			Strike();
		}
	}
	if (Moment.Phase != EHollowArcPhase::Off)
	{
		Timers.SetTimer(PhaseTimer, this, &ThisClass::ApplyPhase, static_cast<float>(Moment.SecondsToNext) + PastBoundarySeconds, /*bLoop=*/false);
	}
}

void AHollowArcHazard::Crackle()
{
	if (!Definition || ShownPhase != EHollowArcPhase::Telegraph)
	{
		return;
	}
	bCrackleLit = !bCrackleLit;
	Glow->SetIntensity(GlowIntensity * Definition->TelegraphGlow);
	Glow->SetVisibility(bCrackleLit);
	GetWorldTimerManager().SetTimer(CrackleTimer, this, &ThisClass::Crackle, 1.f / FMath::Max(Definition->CrackleHz, 1.f), /*bLoop=*/false);
}

int32 AHollowArcHazard::Strike()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !Definition || !Definition->DamageEffect)
	{
		return 0;
	}
	++Strikes;
	TArray<APawn*> Inside;
	HollowAIPlayers::ForEach(World, [this, &Inside](APawn& Player)
	{
		if (Contains(Player.GetActorLocation()))
		{
			Inside.Add(&Player);
		}
	});
	Inside.Sort([](const APawn& A, const APawn& B) { return A.GetName() < B.GetName(); });   // logs and tests
	for (APawn* Player : Inside)
	{
		UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
		if (!Target)
		{
			continue;
		}
		FGameplayEffectContextHandle Context = Target->MakeEffectContext();
		Context.AddSourceObject(this);   // no ability system on a world object (CLAUDE.md sec. 3, Q4)
		const FGameplayEffectSpecHandle Spec = Target->MakeOutgoingSpec(Definition->DamageEffect, 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_Damage, Definition->Damage);
			Target->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
		UE_LOG(LogHollowGame, Log, TEXT("%s strikes %s (health %.0f)"), *GetName(), *Player->GetName(),
			Target->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute()));
	}
	UE_CLOG(Inside.IsEmpty(), LogHollowGame, Log, TEXT("%s strikes an empty passage"), *GetName());
	return Inside.Num();
}

FString AHollowArcHazard::Describe() const
{
	const FHollowArcMoment Moment = GetMoment();
	FString Players;
	HollowAIPlayers::ForEach(GetWorld(), [this, &Players](APawn& Player)
	{
		const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Player);
		Players += FString::Printf(TEXT(" %s%s health %.0f;"), *Player.GetName(), Contains(Player.GetActorLocation()) ? TEXT(" INSIDE") : TEXT(""),
			Abilities ? Abilities->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute()) : 0.f);
	});
	return FString::Printf(TEXT("%s at %s, power %s, %s (%.1f s to next), breaker %s%s, strikes %d |%s"), *GetName(), *GetActorLocation().ToCompactString(),
		ArcState.bPowered ? TEXT("on") : TEXT("off"), *HollowEnumToString(Moment.Phase), Moment.SecondsToNext, *GetNameSafe(Breaker),
		Breaker ? (Breaker->IsOn() ? TEXT(" (on)") : TEXT(" (off)")) : TEXT(""), Strikes, *Players);
}
