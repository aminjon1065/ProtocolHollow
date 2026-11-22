#include "Character/HollowExoFrameCharacter.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Audio/HollowFootstepAudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Character/HollowFootstepNoiseComponent.h"
#include "Carry/HollowCarrierComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/HollowPlayerState.h"
#include "Framework/HollowRaidGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "HollowGameLog.h"
#include "Input/HollowInputConfig.h"
#include "InputActionValue.h"
#include "Interaction/HollowInteractionComponent.h"
#include "Interfaces/HollowCarryReceiver.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Raid/HollowRaidFlowComponent.h"
#include "Raid/HollowRaidSettings.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "Tools/HollowToolMountComponent.h"
#include "Voice/HollowVoiceTalker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowExoFrameCharacter)

const FName AHollowExoFrameCharacter::CameraSocketName(TEXT("head"));

namespace HollowExoFramePrivate
{
	/** Gameplay ability system levels start at 1; state effects have no levels. */
	constexpr float EffectLevel = 1.f;
}

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

	HelmetLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HelmetLight"));
	HelmetLight->SetupAttachment(FirstPersonCamera);

	// The tool in hand (ADR-0027): hand sockets, the default tool and each tool's mesh are data.
	FirstPersonToolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonToolMesh"));
	FirstPersonToolMesh->SetupAttachment(GetRootComponent());   // into the hands once they have a mesh (AttachToHands)
	ToolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ToolMesh"));
	ToolMesh->SetupAttachment(GetRootComponent());
	ToolMount = CreateDefaultSubobject<UHollowToolMountComponent>(TEXT("ToolMount"));
	ToolMount->Setup(FirstPersonToolMesh, ToolMesh, FirstPersonCamera);

	InteractionComponent = CreateDefaultSubobject<UHollowInteractionComponent>(TEXT("Interaction"));
	CarrierComponent = CreateDefaultSubobject<UHollowCarrierComponent>(TEXT("Carrier"));
	FootstepNoise = CreateDefaultSubobject<UHollowFootstepNoiseComponent>(TEXT("FootstepNoise"));
	FootstepAudio = CreateDefaultSubobject<UHollowFootstepAudioComponent>(TEXT("FootstepAudio"));
	FootstepAudio->SoundTag = HollowTags::Noise_Footstep;   // a player's steps are the noise the AI hears
}

void AHollowExoFrameCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	bHelmetLampCastsShadows = HelmetLight->CastShadows;
	ApplyHelmetShadows();   // a copy no one on this machine controls (yet)
	CarrierComponent->SetAttachTargets(FirstPersonCamera, GetMesh());
	ToolMount->AttachToHands(FirstPersonMesh, GetMesh());
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
	UnbindHealth();
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
	HollowPlayerState->GetVoiceTalker()->RefreshPlacement();   // the voice comes from this helmet now (S4-07)

	// The server learns of death from the health set (on the player state); clients follow the replicated flag.
	const UHollowAbilitySystemComponent* AbilitySystem = GetHollowAbilitySystemComponent();
	const UHollowHealthSet* Health = AbilitySystem ? AbilitySystem->GetSet<UHollowHealthSet>() : nullptr;
	if (HasAuthority() && Health && BoundHealthSet.Get() != Health)
	{
		UnbindHealth();
		OutOfHealthHandle = Health->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
		BoundHealthSet = Health;
	}
}

void AHollowExoFrameCharacter::UnbindHealth()
{
	if (const UHollowHealthSet* Health = BoundHealthSet.Get())
	{
		Health->OnOutOfHealth.Remove(OutOfHealthHandle);
	}
	BoundHealthSet = nullptr;
	OutOfHealthHandle.Reset();
}

void AHollowExoFrameCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHollowExoFrameCharacter, bDead);
	FDoRepLifetimeParams LampParams;
	LampParams.bIsPushBased = true;
	LampParams.Condition = COND_SkipOwner;   // the owner predicts its own lamp
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowExoFrameCharacter, bHelmetLampOn, LampParams);
}

void AHollowExoFrameCharacter::HandleOutOfHealth(AActor* EffectCauser, AActor* EffectInstigator, const float Damage)
{
	UE_LOG(LogHollowGame, Log, TEXT("%s: out of health (%.0f damage from %s)"), *GetName(), Damage, *GetNameSafe(EffectInstigator));
	Die();
}

void AHollowExoFrameCharacter::Die()
{
	if (!HasAuthority() || bDead)
	{
		return;
	}
	bDead = true;
	ApplyDeadPresentation();
	if (UHollowAbilitySystemComponent* AbilitySystem = GetHollowAbilitySystemComponent())
	{
		AbilitySystem->CancelAllAbilities();
		if (DeathEffect)
		{
			AbilitySystem->ApplyGameplayEffectToSelf(DeathEffect.GetDefaultObject(), HollowExoFramePrivate::EffectLevel, AbilitySystem->MakeEffectContext());
		}
	}
	CarrierComponent->ServerDrop(/*bForce=*/true);   // the core stays where its carrier fell
	UE_LOG(LogHollowGame, Log, TEXT("%s died"), *GetName());
	if (AHollowRaidGameMode* Raid = GetWorld()->GetAuthGameMode<AHollowRaidGameMode>())
	{
		Raid->GetRaidFlow()->HandlePlayerDied(GetController());
	}
	GetWorldTimerManager().SetTimer(SpectateTimer, this, &ThisClass::BecomeSpectator,
		FMath::Max(GetDefault<UHollowRaidSettings>()->DeathSpectateDelaySeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

void AHollowExoFrameCharacter::OnRep_Dead()
{
	if (bDead)
	{
		ApplyDeadPresentation();
	}
}

void AHollowExoFrameCharacter::ApplyDeadPresentation()
{
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ApplyHelmetLamp(/*bClick=*/false);   // a dead helmet is dark
	if (const AHollowPlayerState* HollowPlayerState = GetPlayerState<AHollowPlayerState>())
	{
		HollowPlayerState->GetVoiceTalker()->RefreshPlacement();   // the dead talk on the radio (S4-07)
	}
}

void AHollowExoFrameCharacter::SetHelmetLampOn(const bool bOn)
{
	if (bHelmetLampOn == bOn)
	{
		return;
	}
	bHelmetLampOn = bOn;
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AHollowExoFrameCharacter, bHelmetLampOn, this);
	}
	UE_LOG(LogHollowGame, Log, TEXT("%s: helmet lamp %s"), *GetName(), bOn ? TEXT("on") : TEXT("off"));
	ApplyHelmetLamp(/*bClick=*/true);
}

void AHollowExoFrameCharacter::OnRep_HelmetLamp()
{
	UE_LOG(LogHollowGame, Log, TEXT("%s: helmet lamp %s (replicated)"), *GetName(), bHelmetLampOn ? TEXT("on") : TEXT("off"));
	ApplyHelmetLamp(/*bClick=*/HasActorBegunPlay());   // a first bunch brings an old switch, not a click
}

void AHollowExoFrameCharacter::ApplyHelmetLamp(const bool bClick)
{
	HelmetLight->SetVisibility(bHelmetLampOn && !bDead);
	OnHelmetLampChanged.Broadcast(bHelmetLampOn);
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	if (bClick && Audio && !bDead && HasActorBegunPlay())
	{
		Audio->PlayAttached(HollowTags::Sound_HelmetLamp_Toggle, FirstPersonCamera);
	}
}

bool AHollowExoFrameCharacter::GetLightCone(FHollowLightCone& OutCone) const
{
	if (bDead || !bHelmetLampOn || HelmetLight->Intensity <= 0.f)
	{
		return false;
	}
	OutCone.Origin = GetPawnViewLocation();
	OutCone.Direction = GetBaseAimRotation().Vector();
	OutCone.HalfAngleDegrees = HelmetLight->OuterConeAngle;
	OutCone.Range = HelmetLight->AttenuationRadius;
	return true;
}

void AHollowExoFrameCharacter::ApplyHelmetShadows()
{
	const bool bOwn = IsLocallyControlled() && Cast<APlayerController>(GetController()) != nullptr;
	HelmetLight->SetCastShadows(bHelmetLampCastsShadows && (bOwn || bOtherHelmetLampsCastShadows));
}

void AHollowExoFrameCharacter::BecomeSpectator()
{
	if (APlayerController* Player = Cast<APlayerController>(GetController()))
	{
		UE_LOG(LogHollowGame, Log, TEXT("%s: %s spectates"), *GetName(), *Player->GetName());
		Player->ChangeState(NAME_Spectating);
		Player->ClientGotoState(NAME_Spectating);
	}
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
	// IsPlayerControlled() needs the player state, which a client may receive after the controller: check the controller type.
	InteractionComponent->SetLocalFocusEnabled(IsLocallyControlled() && Cast<APlayerController>(GetController()) != nullptr);
	ApplyHelmetShadows();

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
	InputConfig->BindNativeAction(*EnhancedInput, HollowTags::Input_Interact, ETriggerEvent::Started, this, &ThisClass::HandleInteract);

	for (const FHollowInputAction& Entry : InputConfig->AbilityActions)
	{
		if (Entry.InputAction && Entry.InputTag.IsValid())
		{
			EnhancedInput->BindAction(Entry.InputAction.Get(), ETriggerEvent::Started, this, &ThisClass::HandleAbilityInputPressed, Entry.InputTag);
			EnhancedInput->BindAction(Entry.InputAction.Get(), ETriggerEvent::Completed, this, &ThisClass::HandleAbilityInputReleased, Entry.InputTag);
			EnhancedInput->BindAction(Entry.InputAction.Get(), ETriggerEvent::Canceled, this, &ThisClass::HandleAbilityInputReleased, Entry.InputTag);
		}
	}
}

void AHollowExoFrameCharacter::HandleAbilityInputPressed(const FGameplayTag InputTag)
{
	if (UHollowAbilitySystemComponent* AbilitySystem = Cast<UHollowAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		AbilitySystem->AbilityInputTagPressed(InputTag);
	}
}

void AHollowExoFrameCharacter::HandleAbilityInputReleased(const FGameplayTag InputTag)
{
	if (UHollowAbilitySystemComponent* AbilitySystem = Cast<UHollowAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		AbilitySystem->AbilityInputTagReleased(InputTag);
	}
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

void AHollowExoFrameCharacter::HandleInteract()
{
	RequestInteract();
}

void AHollowExoFrameCharacter::RequestInteract()
{
	AActor* Focus = InteractionComponent->GetFocusedActor();
	// A carry receiver in focus (the Purification Altar, S7-02) takes the load: interact there instead of dropping it.
	if (Cast<IHollowCarryReceiver>(Focus) || !CarrierComponent->HandleInteractInput(Focus))
	{
		InteractionComponent->RequestInteract();
	}
}
