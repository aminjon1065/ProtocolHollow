#include "Gadgets/HollowPulseMine.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Gadgets/HollowPulseMineDefinition.h"
#include "HollowGameLog.h"
#include "Net/UnrealNetwork.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPulseMine)

AHollowPulseMine::AHollowPulseMine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);   // it lies where it was placed

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // nobody trips over it, traces see through it
	Body->SetCanEverAffectNavigation(false);
	SetRootComponent(Body);

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Body);
	Flash->SetVisibility(false);
	Flash->SetCastShadows(false);
}

void AHollowPulseMine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHollowPulseMine, bDetonated);
}

void AHollowPulseMine::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	if (!ensureMsgf(Definition, TEXT("%s has no pulse mine definition."), *GetName()))
	{
		Destroy();
		return;
	}
	GetWorldTimerManager().SetTimer(ArmTimer, this, &ThisClass::Arm, FMath::Max(Definition->ArmSeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

void AHollowPulseMine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())   // null when the garbage collector ends play
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AHollowPulseMine::Arm()
{
	bArmed = true;
	UE_LOG(LogHollowGame, Log, TEXT("%s of %s is armed"), *GetName(), *GetNameSafe(GetInstigator()));
	GetWorldTimerManager().SetTimer(CheckTimer, this, &ThisClass::CheckTrigger, Definition->CheckSeconds, /*bLoop=*/true);
}

void AHollowPulseMine::CheckTrigger()
{
	if (!FindEnemies(Definition->TriggerRadius).IsEmpty())
	{
		Detonate();
	}
}

TArray<AHollowEnemyCharacter*> AHollowPulseMine::FindEnemies(const float Radius) const
{
	TArray<AHollowEnemyCharacter*> Enemies;
	const UWorld* World = GetWorld();
	if (!World || Radius <= 0.f)
	{
		return Enemies;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowPulseMine), /*bTraceComplex=*/false, this);
	World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AHollowEnemyCharacter* Enemy = Cast<AHollowEnemyCharacter>(Overlap.GetActor());
		if (!Enemy || Enemy->IsDead() || Enemy->IsInVent() || Enemies.Contains(Enemy))
		{
			continue;
		}
		FCollisionQueryParams Sight(SCENE_QUERY_STAT(HollowPulseMineSight), /*bTraceComplex=*/false, this);
		Sight.AddIgnoredActor(Enemy);
		Sight.AddIgnoredActor(GetInstigator());
		const FVector From = GetActorLocation() + FVector(0.0, 0.0, Definition ? Definition->SightHeight : 0.f);   // off the floor it lies on
		if (!World->LineTraceTestByChannel(From, Enemy->GetActorLocation(), ECC_Visibility, Sight))
		{
			Enemies.Add(Enemy);
		}
	}
	// Overlap order is the physics scene's: sort for logs and tests.
	Enemies.Sort([](const AHollowEnemyCharacter& A, const AHollowEnemyCharacter& B) { return A.GetName() < B.GetName(); });
	return Enemies;
}

int32 AHollowPulseMine::Detonate()
{
	if (!HasAuthority() || bDetonated || !Definition)
	{
		return 0;
	}
	GetWorldTimerManager().ClearTimer(ArmTimer);
	GetWorldTimerManager().ClearTimer(CheckTimer);
	bDetonated = true;
	ApplyDetonatedPresentation();
	ForceNetUpdate();

	const TArray<AHollowEnemyCharacter*> Enemies = FindEnemies(Definition->BlastRadius);
	UAbilitySystemComponent* Source = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetInstigator());
	int32 Stunned = 0;
	for (AHollowEnemyCharacter* Enemy : Enemies)
	{
		UAbilitySystemComponent* Target = Enemy->GetAbilitySystemComponent();
		UAbilitySystemComponent* Applier = Source ? Source : Target;
		if (!Target || !Definition->StunEffect)
		{
			continue;
		}
		FGameplayEffectContextHandle Context = Applier->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = Applier->MakeOutgoingSpec(Definition->StunEffect, 1.f, Context);
		if (Spec.IsValid() && Applier->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target).WasSuccessfullyApplied())
		{
			++Stunned;
		}
	}
	if (UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this); Noise && Definition->BlastNoise.IsValid())
	{
		Noise->ReportNoise(FHollowNoiseEvent(Definition->BlastNoise, GetActorLocation(), GetInstigator()));
	}
	UE_LOG(LogHollowGame, Log, TEXT("%s of %s goes off: %d of %d enemies stunned"), *GetName(), *GetNameSafe(GetInstigator()), Stunned, Enemies.Num());
	SetLifeSpan(Definition->FlashSeconds);
	return Stunned;
}

void AHollowPulseMine::OnRep_Detonated()
{
	UE_LOG(LogHollowGame, Log, TEXT("%s: flash (replicated)"), *GetName());
	ApplyDetonatedPresentation();
}

void AHollowPulseMine::ApplyDetonatedPresentation()
{
	Flash->SetVisibility(bDetonated);
	Body->SetVisibility(!bDetonated);
}
