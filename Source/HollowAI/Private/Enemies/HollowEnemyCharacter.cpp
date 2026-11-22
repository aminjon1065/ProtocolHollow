#include "Enemies/HollowEnemyCharacter.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowMovementSet.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Audio/HollowFootstepAudioComponent.h"
#include "Audio/HollowSoundBank.h"
#include "Brain/HollowAIController.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/HollowEnemyDefinition.h"
#include "Enemies/HollowEnemyMontageComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "HollowAILog.h"
#include "Net/UnrealNetwork.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEnemyCharacter)

AHollowEnemyCharacter::AHollowEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystem = CreateDefaultSubobject<UHollowAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	HealthSet = CreateDefaultSubobject<UHollowHealthSet>(TEXT("HealthSet"));   // found by the ability system as its subobject
	MovementSet = CreateDefaultSubobject<UHollowMovementSet>(TEXT("MovementSet"));
	FootstepAudio = CreateDefaultSubobject<UHollowFootstepAudioComponent>(TEXT("FootstepAudio"));
	Montages = CreateDefaultSubobject<UHollowEnemyMontageComponent>(TEXT("Montages"));

	AIControllerClass = AHollowAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Facing follows the controller (its path, or a focus while it strikes), turned at the definition's rate.
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
}

void AHollowEnemyCharacter::PostInitializeComponents()
{
	// Before Super: it spawns the AI controller, whose possession grants the abilities.
	InitializeAbilitySystem(AbilitySystem, this);
	Super::PostInitializeComponents();
	if (HasAuthority())
	{
		OutOfHealthHandle = HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
	}
	UE_CLOG(!Definition, LogHollowAI, Error, TEXT("%s: enemy has no definition."), *GetName());
	Montages->SetDefinition(Definition);
}

void AHollowEnemyCharacter::BeginPlay()
{
	if (Definition)
	{
		FootstepAudio->SoundTag = Definition->FootstepSound;   // before the components begin play
		FootstepAudio->Cadence = Definition->FootstepCadence;
	}
	Super::BeginPlay();
	if (Definition)
	{
		GetCharacterMovement()->RotationRate = FRotator(0.0, Definition->TurnRate, 0.0);
		GetCharacterMovement()->MaxWalkSpeed = Definition->PatrolSpeed;
		if (HasAuthority())
		{
			SetNetUpdateFrequency(Definition->NetUpdateFrequency);
		}
	}
	// Clients: it comes into relevancy (a stalker hidden in a vent is not relevant to anyone, so it appears anew).
	UE_CLOG(!HasAuthority(), LogHollowAI, Log, TEXT("%s: appears at %s (replicated)"), *GetName(), *GetActorLocation().ToCompactString());
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!HasAuthority() && !bInVent && GameState && GetDefault<UHollowAudioSettings>()->IsFresh(VentExitServerTime, GameState->GetServerWorldTimeSeconds()))
	{
		PresentVentExit();   // it has just come out of a vent here
	}
}

void AHollowEnemyCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (Definition && Definition->AbilitySet && GrantedAbilities.IsEmpty())
	{
		Definition->AbilitySet->GiveToAbilitySystem(AbilitySystem, &GrantedAbilities, this);
	}
}

void AHollowEnemyCharacter::UnPossessed()
{
	GrantedAbilities.TakeFromAbilitySystem(AbilitySystem);
	Super::UnPossessed();
}

void AHollowEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHollowEnemyCharacter, bDead);
	DOREPLIFETIME(AHollowEnemyCharacter, bInVent);
	DOREPLIFETIME_CONDITION(AHollowEnemyCharacter, VentExitServerTime, COND_InitialOnly);
}

void AHollowEnemyCharacter::HandleOutOfHealth(AActor* EffectCauser, AActor* EffectInstigator, const float Damage)
{
	Die();
}

void AHollowEnemyCharacter::Die()
{
	if (!HasAuthority() || bDead)
	{
		return;
	}
	bDead = true;
	AbilitySystem->CancelAllAbilities();
	if (AHollowAIController* Brain = Cast<AHollowAIController>(GetController()))
	{
		Brain->Shutdown();
	}
	ApplyDeadPresentation();
	if (Definition)
	{
		SetLifeSpan(Definition->DeathLingerSeconds);
	}
	UE_LOG(LogHollowAI, Log, TEXT("%s died"), *GetName());
}

void AHollowEnemyCharacter::OnRep_Dead()
{
	if (bDead)
	{
		ApplyDeadPresentation();
	}
}

void AHollowEnemyCharacter::ApplyDeadPresentation()
{
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);   // the body never blocks the living
}

void AHollowEnemyCharacter::SetInVent(const bool bInside)
{
	if (!HasAuthority() || bInVent == bInside)
	{
		return;
	}
	bInVent = bInside;
	ApplyVentPresentation(/*bAnimate=*/true);
	if (bInside)
	{
		PlayVentSound();
	}
	else
	{
		MarkVentExit();
	}
	ForceNetUpdate();
}

void AHollowEnemyCharacter::EmergeFromVent()
{
	if (HasAuthority() && !bInVent)
	{
		MarkVentExit();
	}
}

void AHollowEnemyCharacter::MarkVentExit()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	VentExitServerTime = static_cast<float>(GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds());
	PresentVentExit();
}

void AHollowEnemyCharacter::OnRep_InVent()
{
	UE_LOG(LogHollowAI, Log, TEXT("%s: %s the vents (replicated)"), *GetName(), bInVent ? TEXT("into") : TEXT("out of"));
	ApplyVentPresentation(/*bAnimate=*/HasActorBegunPlay());
	if (HasActorBegunPlay())   // not from a first bunch: BeginPlay decides that one
	{
		if (bInVent)
		{
			PlayVentSound();
		}
		else
		{
			PresentVentExit();
		}
	}
}

void AHollowEnemyCharacter::PlayVentSound() const
{
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
	{
		Audio->Play(HollowTags::Sound_Stalker_Vent, GetActorLocation());   // the grille rattles where it goes in or comes out
	}
}

void AHollowEnemyCharacter::PresentVentExit()
{
	PlayVentSound();
	Montages->Play(EHollowEnemyMontage::VentExit);
}

void AHollowEnemyCharacter::ApplyVentPresentation(const bool bAnimate)
{
	SetActorEnableCollision(!bInVent);   // nobody bumps into what is not there, client copies included
	GetWorldTimerManager().ClearTimer(VentHideTimer);
	if (bInVent)
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		// Seen going in: it hides once its montage has played (still relevant meanwhile: hidden and without collision
		// it would not be). No montage, or a late first bunch: at once.
		const float Showing = bAnimate ? Montages->Play(EHollowEnemyMontage::VentEnter) : 0.f;
		if (Showing > 0.f)
		{
			GetWorldTimerManager().SetTimer(VentHideTimer, FTimerDelegate::CreateWeakLambda(this, [this] { SetActorHiddenInGame(bInVent); }), Showing, /*bLoop=*/false);
		}
		else
		{
			SetActorHiddenInGame(true);
		}
		return;
	}
	SetActorHiddenInGame(false);
	if (!bDead)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}
