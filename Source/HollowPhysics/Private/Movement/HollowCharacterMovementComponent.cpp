#include "Movement/HollowCharacterMovementComponent.h"

#include "Carry/HollowCarrySettings.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HollowPhysicsLog.h"
#include "Misc/DataValidation.h"
#include "Movement/HollowMovementMath.h"
#include "Movement/HollowMovementTuning.h"
#include "Movement/HollowSavedMove.h"
#include "Util/HollowEnumUtils.h"

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
	const FHollowCarryModeTuning* Carry = GetCarryTuning();
	return Tuning && IsMovingOnGround() && UpdatedComponent && (!Carry || Carry->bAllowSprint)
		&& HollowMovementMath::CanSprint(Tuning->Ground, bWantsToSprint, IsCrouching(), Acceleration, UpdatedComponent->GetForwardVector());
}

void UHollowCharacterMovementComponent::SetServerCarry(const EHollowCarryClaim Claim)
{
	if (Claim == ServerCarry.Current)
	{
		return;
	}
	ServerCarry.Previous = ServerCarry.Current;
	ServerCarry.Current = Claim;
	ServerCarry.ChangedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

EHollowCarryClaim UHollowCharacterMovementComponent::GetEffectiveCarry() const
{
	// Only the server simulating a remote carrier's moves needs the floor: everywhere else the claim is this machine's
	// own knowledge (listen host and AI claim the server state directly, the owning client predicts it).
	if (!Tuning || !CharacterOwner || !CharacterOwner->HasAuthority() || CharacterOwner->IsLocallyControlled())
	{
		return ClaimedCarry;
	}
	const double Age = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - ServerCarry.ChangedAt;
	return HollowCarryPolicy::Effective(ClaimedCarry, ServerCarry, Age, GetDefault<UHollowCarrySettings>()->ServerSlowdownGraceSeconds, Tuning->Carry);
}

const FHollowCarryModeTuning* UHollowCharacterMovementComponent::GetCarryTuning() const
{
	return Tuning ? Tuning->Carry.Find(GetEffectiveCarry()) : nullptr;
}

bool UHollowCharacterMovementComponent::IsCarryFloorEnforced() const
{
	return GetEffectiveCarry() != ClaimedCarry;
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
	const FHollowCarryModeTuning* Carry = GetCarryTuning();
	const float CarryScale = Carry ? Carry->SpeedScale : 1.f;
	if (!Tuning || !IsMovingOnGround() || !UpdatedComponent)
	{
		return BaseSpeed * CarryScale;
	}
	const bool bSprintRequest = bWantsToSprint && (!Carry || Carry->bAllowSprint);
	return BaseSpeed * CarryScale * HollowMovementMath::GetSpeedMultiplier(Tuning->Ground, bSprintRequest, IsCrouching(), Acceleration,
		UpdatedComponent->GetForwardVector());
}

float UHollowCharacterMovementComponent::GetMaxAcceleration() const
{
	const FHollowCarryModeTuning* Carry = GetCarryTuning();
	return Super::GetMaxAcceleration() * (Carry ? Carry->AccelerationScale : 1.f);
}

bool UHollowCharacterMovementComponent::CanAttemptJump() const
{
	const FHollowCarryModeTuning* Carry = GetCarryTuning();
	return Super::CanAttemptJump() && (!Carry || Carry->bAllowJump);
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
	const EHollowCarryClaim PreviousClaim = ClaimedCarry;
	ClaimedCarry = HollowMoveFlags::DecodeCarry(Flags);
	UE_CLOG(ClaimedCarry != PreviousClaim, LogHollowPhysics, Verbose, TEXT("%s: move claims %s (was %s)"), *GetNameSafe(CharacterOwner),
		*HollowEnumToString(ClaimedCarry), *HollowEnumToString(PreviousClaim));

	// Server: a remote carrier moving on the floor instead of its own claim gets corrected; log the start and the end.
	if (CharacterOwner && CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled())
	{
		const bool bEnforced = IsCarryFloorEnforced();
		UE_CLOG(bEnforced != bCarryFloorEnforced, LogHollowPhysics, Log, TEXT("%s: carry claim %s, server %s -> moves use %s"),
			*GetNameSafe(CharacterOwner), *HollowEnumToString(ClaimedCarry), *HollowEnumToString(ServerCarry.Current),
			*HollowEnumToString(GetEffectiveCarry()));
		bCarryFloorEnforced = bEnforced;
	}
}

bool UHollowCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	// Replaying saved moves decodes their flags; keep the live requests like the engine does for crouch/jump.
	const bool bRealWantsToSprint = bWantsToSprint;
	const EHollowCarryClaim RealClaimedCarry = ClaimedCarry;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealWantsToSprint;
	ClaimedCarry = RealClaimedCarry;
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
	SavedClaimedCarry = EHollowCarryClaim::None;
}

void FSavedMove_Hollow::SetMoveFor(ACharacter* Character, const float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	if (const UHollowCharacterMovementComponent* Movement = Cast<UHollowCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Movement->WantsToSprint();
		SavedClaimedCarry = Movement->GetClaimedCarry();
	}
}

uint8 FSavedMove_Hollow::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Flags |= HollowMoveFlags::Sprint;
	}
	return Flags | HollowMoveFlags::EncodeCarry(SavedClaimedCarry);
}

bool FSavedMove_Hollow::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, const float MaxDelta) const
{
	// Every saved move of a Hollow character is an FSavedMove_Hollow (FNetworkPredictionData_Client_Hollow).
	const FSavedMove_Hollow* Other = static_cast<const FSavedMove_Hollow*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || SavedClaimedCarry != Other->SavedClaimedCarry)
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
