#include "Movement/HollowCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "Misc/DataValidation.h"
#include "Movement/HollowMovementMath.h"
#include "Movement/HollowMovementTuning.h"
#include "Movement/HollowSavedMove.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCharacterMovementComponent)

#define LOCTEXT_NAMESPACE "HollowCharacterMovement"

// --- UHollowCharacterMovementComponent ---------------------------------------------------------------------------

UHollowCharacterMovementComponent::UHollowCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetNavAgentPropertiesRef().bCanCrouch = true;
}

bool UHollowCharacterMovementComponent::IsSprinting() const
{
	return Tuning && IsMovingOnGround() && UpdatedComponent
		&& HollowMovementMath::CanSprint(Tuning->Ground, bWantsToSprint, IsCrouching(), Acceleration, UpdatedComponent->GetForwardVector());
}

void UHollowCharacterMovementComponent::SetMovementTuning(const UHollowMovementTuning* InTuning)
{
	Tuning = InTuning;
	if (Tuning)
	{
		ApplyTuning(*Tuning);
	}
}

void UHollowCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	if (Tuning)
	{
		ApplyTuning(*Tuning);
	}
}

float UHollowCharacterMovementComponent::GetMaxSpeed() const
{
	const float BaseSpeed = Super::GetMaxSpeed();   // walk or crouch speed from the applied tuning
	if (!Tuning || !IsMovingOnGround() || !UpdatedComponent)
	{
		return BaseSpeed;
	}
	return BaseSpeed * HollowMovementMath::GetSpeedMultiplier(Tuning->Ground, bWantsToSprint, IsCrouching(), Acceleration,
		UpdatedComponent->GetForwardVector());
}

FNetworkPredictionData_Client* UHollowCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UHollowCharacterMovementComponent* MutableThis = const_cast<UHollowCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Hollow(*this);   // owned and deleted by the base class
	}
	return ClientPredictionData;
}

#if WITH_EDITOR
EDataValidationResult UHollowCharacterMovementComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!Tuning && !HasAnyFlags(RF_ClassDefaultObject))
	{
		Context.AddError(LOCTEXT("MissingTuning", "Movement tuning is not set: assign a HollowMovementTuning data asset."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

void UHollowCharacterMovementComponent::UpdateFromCompressedFlags(const uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & HollowMoveFlags::Sprint) != 0;
}

bool UHollowCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	// Replaying saved moves decodes their flags; keep the live input request like the engine does for crouch/jump.
	const bool bRealWantsToSprint = bWantsToSprint;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealWantsToSprint;
	return bResult;
}

void UHollowCharacterMovementComponent::ApplyTuning(const UHollowMovementTuning& InTuning)
{
	MaxWalkSpeed = InTuning.Ground.WalkSpeed;
	MaxWalkSpeedCrouched = InTuning.Ground.CrouchSpeed;
	MaxAcceleration = InTuning.MaxAcceleration;
	BrakingDecelerationWalking = InTuning.BrakingDeceleration;
	GroundFriction = InTuning.GroundFriction;
	JumpZVelocity = InTuning.JumpZVelocity;
	AirControl = InTuning.AirControl;
	BrakingDecelerationFalling = InTuning.BrakingDecelerationFalling;
}

// --- FSavedMove_Hollow -------------------------------------------------------------------------------------------

void FSavedMove_Hollow::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
}

void FSavedMove_Hollow::SetMoveFor(ACharacter* Character, const float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	if (const UHollowCharacterMovementComponent* Movement = Cast<UHollowCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Movement->WantsToSprint();
	}
}

uint8 FSavedMove_Hollow::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Flags |= HollowMoveFlags::Sprint;
	}
	return Flags;
}

bool FSavedMove_Hollow::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, const float MaxDelta) const
{
	// Every saved move of a Hollow character is an FSavedMove_Hollow (FNetworkPredictionData_Client_Hollow).
	const FSavedMove_Hollow* Other = static_cast<const FSavedMove_Hollow*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

// --- FNetworkPredictionData_Client_Hollow ------------------------------------------------------------------------

FNetworkPredictionData_Client_Hollow::FNetworkPredictionData_Client_Hollow(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_Hollow::AllocateNewMove()
{
	return MakeShared<FSavedMove_Hollow>();
}

#undef LOCTEXT_NAMESPACE
