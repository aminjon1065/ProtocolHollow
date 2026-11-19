#include "Character/HollowExoFrameCharacter.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/HollowPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HollowGameLog.h"
#include "Input/HollowInputConfig.h"
#include "InputActionValue.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowExoFrameCharacter)

const FName AHollowExoFrameCharacter::CameraSocketName(TEXT("head"));

AHollowExoFrameCharacter::AHollowExoFrameCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UHollowCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Everyone else sees the body; its owner only gets its shadow (also implies bOwnerNoSee).
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	// Offsets, field of view and first-person scale are data (BP_ExoFrame).
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(FirstPersonMesh, CameraSocketName);
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->bEnableFirstPersonFieldOfView = true;
	FirstPersonCamera->bEnableFirstPersonScale = true;
}

void AHollowExoFrameCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);   // assigns the player state
	InitializeFromPlayerState();

	if (UHollowAbilitySystemComponent* AbilitySystem = GetHollowAbilitySystemComponent())
	{
		for (const UHollowAbilitySet* AbilitySet : AbilitySets)
		{
			if (AbilitySet)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystem, &GrantedAbilitySetHandles, this);
			}
		}
	}
}

void AHollowExoFrameCharacter::UnPossessed()
{
	GrantedAbilitySetHandles.TakeFromAbilitySystem(GetHollowAbilitySystemComponent());
	UninitializeAbilitySystem();
	Super::UnPossessed();
}

void AHollowExoFrameCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (GetPlayerState())
	{
		InitializeFromPlayerState();
	}
	else
	{
		UninitializeAbilitySystem();
	}
}

void AHollowExoFrameCharacter::InitializeFromPlayerState()
{
	AHollowPlayerState* HollowPlayerState = GetPlayerState<AHollowPlayerState>();
	if (!HollowPlayerState)
	{
		UE_LOG(LogHollowGame, Error, TEXT("%s: needs an AHollowPlayerState (check the game mode's PlayerStateClass)."), *GetName());
		return;
	}
	InitializeAbilitySystem(HollowPlayerState->GetHollowAbilitySystemComponent(), HollowPlayerState);
}

UHollowCharacterMovementComponent* AHollowExoFrameCharacter::GetHollowMovement() const
{
	return CastChecked<UHollowCharacterMovementComponent>(GetCharacterMovement());   // guaranteed by the constructor
}

void AHollowExoFrameCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	UE_LOG(LogHollowGame, Log, TEXT("%s controlled by %s (%s)"), *GetName(), *GetNameSafe(GetController()),
		IsLocallyControlled() ? TEXT("local") : TEXT("remote"));

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!InputSubsystem)
	{
		return;   // not locally controlled (server-side copy of a remote player, AI)
	}

	if (DefaultMappingContext)
	{
		InputSubsystem->AddMappingContext(DefaultMappingContext, DefaultMappingPriority);
	}
	else
	{
		UE_LOG(LogHollowGame, Error, TEXT("%s: DefaultMappingContext is not set."), *GetName());
	}
}

void AHollowExoFrameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!ensureMsgf(EnhancedInput, TEXT("DefaultInputComponentClass must be EnhancedInputComponent (DefaultInput.ini).")))
	{
		return;
	}
	if (!InputConfig)
	{
		UE_LOG(LogHollowGame, Error, TEXT("%s: InputConfig is not set."), *GetName());
		return;
	}

	InputConfig->BindNativeAction(*EnhancedInput, HollowTags::Input_Move, ETriggerEvent::Triggered, this, &ThisClass::HandleMove);
	InputConfig->BindNativeAction(*EnhancedInput, HollowTags::Input_Look, ETriggerEvent::Triggered, this, &ThisClass::HandleLook);
	InputConfig->BindNativeAction(*EnhancedInput, HollowTags::Input_Jump, ETriggerEvent::Started, this, &ThisClass::HandleJumpStarted);
	InputConfig->BindNativeAction(*EnhancedInput, HollowTags::Input_Jump, ETriggerEvent::Completed, this, &ThisClass::HandleJumpCompleted);
	InputConfig->BindNativeAction(*EnhancedInput, HollowTags::Input_Crouch, ETriggerEvent::Started, this, &ThisClass::HandleCrouchStarted);
	InputConfig->BindNativeAction(*EnhancedInput, HollowTags::Input_Crouch, ETriggerEvent::Completed, this, &ThisClass::HandleCrouchCompleted);
	InputConfig->BindNativeAction(*EnhancedInput, HollowTags::Input_Sprint, ETriggerEvent::Started, this, &ThisClass::HandleSprintStarted);
	InputConfig->BindNativeAction(*EnhancedInput, HollowTags::Input_Sprint, ETriggerEvent::Completed, this, &ThisClass::HandleSprintCompleted);
}

void AHollowExoFrameCharacter::HandleMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AHollowExoFrameCharacter::HandleLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AHollowExoFrameCharacter::HandleJumpStarted()
{
	Jump();
}

void AHollowExoFrameCharacter::HandleJumpCompleted()
{
	StopJumping();
}

void AHollowExoFrameCharacter::HandleCrouchStarted()
{
	Crouch();
}

void AHollowExoFrameCharacter::HandleCrouchCompleted()
{
	UnCrouch();
}

void AHollowExoFrameCharacter::HandleSprintStarted()
{
	GetHollowMovement()->SetWantsToSprint(true);
}

void AHollowExoFrameCharacter::HandleSprintCompleted()
{
	GetHollowMovement()->SetWantsToSprint(false);
}
