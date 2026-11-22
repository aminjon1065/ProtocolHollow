#include "Gadgets/HollowOccultWard.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Gadgets/HollowOccultWardDefinition.h"
#include "HollowGameLog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Tags/HollowGameplayTags.h"
#include "Threat/HollowAIPlayers.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowOccultWard)

namespace HollowOccultWardPrivate
{
	/** The decal projects straight down onto the floor it lies on. */
	const FRotator ProjectDown(-90.0, 0.0, 0.0);
	/** A pulse swings the glow between full and dark around this middle. */
	constexpr double PulseMiddle = 0.5;

	template <typename TActor>
	FString Names(const TArray<TActor*>& Actors)
	{
		return FString::JoinBy(Actors, TEXT(", "), [](const TActor* Actor) { return GetNameSafe(Actor); });
	}
}

AHollowOccultWard::AHollowOccultWard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);   // it lies where it was laid

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Rune = CreateDefaultSubobject<UDecalComponent>(TEXT("Rune"));
	Rune->SetupAttachment(Root);
	Rune->SetRelativeRotation(HollowOccultWardPrivate::ProjectDown);
	Rune->SetVisibility(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);
}

void AHollowOccultWard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHollowOccultWard, PlacedServerTime, COND_InitialOnly);
}

double AHollowOccultWard::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

double AHollowOccultWard::Age() const
{
	return FMath::Max(ServerNow() - PlacedServerTime, 0.0);
}

float AHollowOccultWard::GetSecondsLeft() const
{
	return Definition ? FMath::Max(static_cast<float>(Definition->LifeSeconds - Age()), 0.f) : 0.f;
}

bool AHollowOccultWard::Contains(const FVector& Point) const
{
	if (!Definition)
	{
		return false;
	}
	const FVector Offset = Point - GetActorLocation();
	return FVector2D(Offset.X, Offset.Y).SizeSquared() <= FMath::Square(static_cast<double>(Definition->Radius))
		&& FMath::Abs(Offset.Z) <= Definition->HeightTolerance;
}

float AHollowOccultWard::GetGlowAlpha() const
{
	if (!Definition)
	{
		return 0.f;
	}
	const double Time = Age();
	const double PulseFrom = Definition->LifeSeconds - Definition->PulseSeconds;
	if (Time >= Definition->LifeSeconds)
	{
		return 0.f;
	}
	if (Time < Definition->FadeInSeconds)
	{
		return static_cast<float>(Time / Definition->FadeInSeconds);
	}
	if (Definition->PulseSeconds > 0.f && Time >= PulseFrom)
	{
		// From full brightness down and up again, PulseHz times a second.
		using namespace HollowOccultWardPrivate;
		return static_cast<float>(PulseMiddle + PulseMiddle * FMath::Cos(UE_TWO_PI * (Time - PulseFrom) * Definition->PulseHz));
	}
	return 1.f;
}

void AHollowOccultWard::BeginPlay()
{
	Super::BeginPlay();
	if (!Definition)
	{
		UE_LOG(LogHollowGame, Error, TEXT("%s has no ritual ward definition."), *GetName());
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}
	if (HasAuthority())
	{
		PlacedServerTime = ServerNow();
		GetWorldTimerManager().SetTimer(LifeTimer, this, &ThisClass::Expire, Definition->LifeSeconds, /*bLoop=*/false);
		GetWorldTimerManager().SetTimer(CheckTimer, this, &ThisClass::CheckInside, Definition->CheckSeconds, /*bLoop=*/true);
	}

	// The look, on every machine.
	Rune->DecalSize = FVector(Rune->DecalSize.X, Definition->Radius, Definition->Radius);   // the circle is as wide as it reaches
	RuneMaterial = Rune->CreateDynamicMaterialInstance();
	RuneStrength = RuneMaterial && !StrengthParameter.IsNone() ? RuneMaterial->K2_GetScalarParameterValue(StrengthParameter) : 0.f;
	GlowIntensity = Glow->Intensity;
	Rune->SetVisibility(true);
	Glow->SetVisibility(true);
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this); Audio && Definition->HumSound.IsValid())
	{
		Hum = Audio->PlayAttached(Definition->HumSound, Root);
	}
	bLookStarted = true;
	UpdateLook();
	UE_LOG(LogHollowGame, Log, TEXT("%s of %s burns at %s, %.1f s left (%s)"), *GetName(), *GetNameSafe(GetInstigator()), *GetActorLocation().ToCompactString(),
		GetSecondsLeft(), HasAuthority() ? TEXT("server") : TEXT("replicated"));

	if (HasAuthority())
	{
		CheckInside();   // whoever stands on it from the start
	}
}

void AHollowOccultWard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())   // null when the garbage collector ends play
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (Hum)
	{
		Hum->Stop();
		Hum = nullptr;
	}
	if (bLookStarted)
	{
		UE_LOG(LogHollowGame, Log, TEXT("%s goes out (%s)"), *GetName(), HasAuthority() ? TEXT("server") : TEXT("replicated"));
	}
	Super::EndPlay(EndPlayReason);
}

void AHollowOccultWard::OnRep_PlacedServerTime()
{
	if (bLookStarted)
	{
		UpdateLook();   // the time came after BeginPlay: the look follows it
	}
}

void AHollowOccultWard::Expire()
{
	Destroy();
}

void AHollowOccultWard::UpdateLook()
{
	if (!Definition)
	{
		return;
	}
	const float Alpha = GetGlowAlpha();
	if (RuneMaterial && !StrengthParameter.IsNone())
	{
		RuneMaterial->SetScalarParameterValue(StrengthParameter, RuneStrength * Alpha);
	}
	Glow->SetIntensity(GlowIntensity * Alpha);

	// A timer only while the look changes (coming up, pulsing); in between, one wake-up for the pulse.
	const double Time = Age();
	const double PulseFrom = Definition->LifeSeconds - Definition->PulseSeconds;
	FTimerManager& Timers = GetWorldTimerManager();
	if (Time < Definition->FadeInSeconds || Time >= PulseFrom)
	{
		if (!Timers.IsTimerActive(LookTimer) && LookUpdateHz > 0.f)
		{
			Timers.SetTimer(LookTimer, this, &ThisClass::UpdateLook, 1.f / LookUpdateHz, /*bLoop=*/true);
		}
	}
	else
	{
		Timers.SetTimer(LookTimer, this, &ThisClass::UpdateLook, static_cast<float>(PulseFrom - Time), /*bLoop=*/false);
	}
}

void AHollowOccultWard::CheckInside()
{
	using namespace HollowOccultWardPrivate;
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !Definition)
	{
		return;
	}
	TArray<AHollowEnemyCharacter*> Enemies;
	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowOccultWard), /*bTraceComplex=*/false, this);
	const float Reach = FVector2D(Definition->Radius, Definition->HeightTolerance).Size();
	World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Reach), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AHollowEnemyCharacter* Enemy = Cast<AHollowEnemyCharacter>(Overlap.GetActor());
		if (Enemy && !Enemy->IsDead() && !Enemy->IsInVent() && Contains(Enemy->GetActorLocation()))
		{
			Enemies.AddUnique(Enemy);
		}
	}
	TArray<APawn*> Players;
	HollowAIPlayers::ForEach(World, [this, &Players](APawn& Player)
	{
		if (Contains(Player.GetActorLocation()))
		{
			Players.Add(&Player);
		}
	});
	// Overlap order is the physics scene's: sort for logs and tests.
	Enemies.Sort([](const AHollowEnemyCharacter& A, const AHollowEnemyCharacter& B) { return A.GetName() < B.GetName(); });
	Players.Sort([](const APawn& A, const APawn& B) { return A.GetName() < B.GetName(); });

	// The placer's ability system applies them (its ward); the target's own if the placer is gone.
	UAbilitySystemComponent* Placer = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetInstigator());
	auto Apply = [this, Placer](UAbilitySystemComponent* Target, const TSubclassOf<UGameplayEffect>& Effect, const float Drain)
	{
		UAbilitySystemComponent* Applier = Placer ? Placer : Target;
		if (!Applier || !Target || !Effect)
		{
			return;
		}
		FGameplayEffectContextHandle Context = Applier->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = Applier->MakeOutgoingSpec(Effect, 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_ResonanceDrain, -Drain);
			Applier->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
		}
	};
	const float Drain = Definition->ResonanceDrainPerSecond * Definition->CheckSeconds;
	for (AHollowEnemyCharacter* Enemy : Enemies)
	{
		Apply(Enemy->GetAbilitySystemComponent(), Definition->EnemyEffect, 0.f);
	}
	for (APawn* Player : Players)
	{
		UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
		Apply(Target, Definition->PlayerEffect, 0.f);
		Apply(Target, Definition->DrainEffect, Drain);
	}

	EnemiesInside.Reset();
	EnemiesInside.Append(Enemies);
	PlayersInside.Reset();
	PlayersInside.Append(Players);
	const FString Inside = FString::Printf(TEXT("%d enemies [%s], %d players [%s]"), Enemies.Num(), *Names(Enemies), Players.Num(), *Names(Players));
	if (Inside != LastInside)
	{
		UE_LOG(LogHollowGame, Log, TEXT("%s: inside %s"), *GetName(), *Inside);
		LastInside = Inside;
	}
}

FString AHollowOccultWard::Describe() const
{
	// Who is inside is the server's to know; a client shows only what it sees.
	const FString Inside = HasAuthority() ? FString::Printf(TEXT("%d enemies and %d players inside"), EnemiesInside.Num(), PlayersInside.Num()) : TEXT("inside: server only");
	return FString::Printf(TEXT("%s of %s at %s, %.1f s left, glow %.2f, %s (%s)"), *GetName(), *GetNameSafe(GetInstigator()),
		*GetActorLocation().ToCompactString(), GetSecondsLeft(), GetGlowAlpha(), *Inside, HasAuthority() ? TEXT("server") : TEXT("replicated"));
}
