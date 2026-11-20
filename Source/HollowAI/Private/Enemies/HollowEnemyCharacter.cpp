#include "Enemies/HollowEnemyCharacter.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Brain/HollowAIController.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/HollowEnemyDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HollowAILog.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEnemyCharacter)

AHollowEnemyCharacter::AHollowEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystem = CreateDefaultSubobject<UHollowAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	HealthSet = CreateDefaultSubobject<UHollowHealthSet>(TEXT("HealthSet"));   // found by the ability system as its subobject

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
}

void AHollowEnemyCharacter::BeginPlay()
{
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
