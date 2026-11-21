#include "Movement/HollowCharacterMovementComponent.h"

#include "Carry/HollowCarrySettings.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameNetworkManager.h"
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

bool UHollowCharacterMovementComponent::IsRemoteOnServer() const
{
	return CharacterOwner && CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled();
}

double UHollowCharacterMovementComponent::GetClientTimeStamp() const
{
	const FNetworkPredictionData_Server_Character* ServerData = HasPredictionData_Server() ? GetPredictionData_Server_Character() : nullptr;
	return ServerData ? ServerData->CurrentClientTimeStamp : 0.0;
}

bool UHollowCharacterMovementComponent::GetAcceptedClientMove(FVector& OutLocation, double& OutTimeStamp) const
{
	if (!bServerAcceptClientAuthoritativePosition || !bHasAcceptedClientMove)
	{
		return false;
	}
	OutLocation = AcceptedClientLocation;
	OutTimeStamp = AcceptedClientTimeStamp;
	return true;
}

float UHollowCharacterMovementComponent::GetCarryWalkSpeed(const EHollowCarryClaim Claim) const
{
	return Tuning ? Tuning->Ground.WalkSpeed * Tuning->Carry.GetSpeedScale(Claim) : MaxWalkSpeed;
}

void UHollowCharacterMovementComponent::SetCoopPartner(const APawn* Partner, const FHollowCoopCarryTuning* Tether)
{
	const bool bHadPartner = CoopPartner.IsValid();
	CoopPartner = Partner;
	if (Partner && Tether)
	{
		CoopTether = *Tether;
	}
	if (!IsRemoteOnServer())
	{
		return;   // the listen host and AI are the authority themselves; clients only tether
	}
	if (Partner)
	{
		CoopReturn.Reset();
		if (!bServerAcceptClientAuthoritativePosition)
		{
			bSawCoopClaim = false;
		}
		SetClientAuthority(true);
	}
	else if (bHadPartner && bServerAcceptClientAuthoritativePosition && !CoopReturn.IsSet())
	{
		FHollowCoopReturn Return;
		Return.LeftAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		CoopReturn = Return;
		UE_LOG(LogHollowPhysics, Log, TEXT("%s: left the co-op carry, client-authoritative until its moves agree"), *GetNameSafe(CharacterOwner));
	}
}

void UHollowCharacterMovementComponent::ForceServerAuthority(const FVector& SaneLocation)
{
	CoopReturn.Reset();
	if (!IsRemoteOnServer() || !UpdatedComponent)
	{
		return;
	}
	SetClientAuthority(false);
	// Back where the sanity check last saw it fit; its next checked move gets the correction.
	UpdatedComponent->SetWorldLocation(SaneLocation, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = FVector::ZeroVector;
	bJustTeleported = true;
	UE_LOG(LogHollowPhysics, Warning, TEXT("%s: server-authoritative again, put back at %s"), *GetNameSafe(CharacterOwner), *SaneLocation.ToCompactString());
}

void UHollowCharacterMovementComponent::SetClientAuthority(const bool bClientAuthoritative)
{
	if (bServerAcceptClientAuthoritativePosition == bClientAuthoritative)
	{
		return;
	}
	// ADR-0028: the server takes the client's position and never corrects it (Docs/Architecture/03 sec. 3.5).
	bServerAcceptClientAuthoritativePosition = bClientAuthoritative;
	bHasAcceptedClientMove = false;
	bIgnoreClientMovementErrorChecksAndCorrection = bClientAuthoritative;
	UE_LOG(LogHollowPhysics, Log, TEXT("%s: movement %s"), *GetNameSafe(CharacterOwner),
		bClientAuthoritative ? TEXT("client-authoritative (co-op carry)") : TEXT("server-authoritative again"));
}

void UHollowCharacterMovementComponent::UpdateCoopReturn(const bool bInSync)
{
	if (!CoopReturn.IsSet())
	{
		return;
	}
	const UHollowCarrySettings* Settings = GetDefault<UHollowCarrySettings>();
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (CoopReturn->ShouldReturn(Now, bInSync, Settings->CoopAuthoritySettleSeconds, Settings->CoopAuthorityTimeoutSeconds))
	{
		UE_CLOG(!bInSync, LogHollowPhysics, Warning, TEXT("%s: back to server authority after %.2f s without agreeing moves"), *GetNameSafe(CharacterOwner),
			Now - CoopReturn->LeftAt);
		CoopReturn.Reset();
		SetClientAuthority(false);
	}
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

void UHollowCharacterMovementComponent::CalcVelocity(const float DeltaTime, const float Friction, const bool bFluid, const float BrakingDeceleration)
{
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
	// The tether, by whoever controls this carrier, on the ground, in moves that carry together (replays of older
	// moves keep their own claim). A remote carrier on the server is client-authoritative meanwhile (ADR-0028).
	const APawn* Partner = CoopPartner.Get();
	if (Partner && UpdatedComponent && CharacterOwner && CharacterOwner->IsLocallyControlled() && IsMovingOnGround()
		&& GetEffectiveCarry() == EHollowCarryClaim::Coop)
	{
		Velocity = HollowCarryTether::Constrain(Velocity, UpdatedComponent->GetComponentLocation(), Partner->GetActorLocation(), CoopTether, GetMaxSpeed());
	}
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
	if (IsRemoteOnServer())
	{
		const bool bEnforced = IsCarryFloorEnforced();
		UE_CLOG(bEnforced != bCarryFloorEnforced && !bServerAcceptClientAuthoritativePosition, LogHollowPhysics, Log,
			TEXT("%s: carry claim %s, server %s -> moves use %s"), *GetNameSafe(CharacterOwner), *HollowEnumToString(ClaimedCarry),
			*HollowEnumToString(ServerCarry.Current), *HollowEnumToString(GetEffectiveCarry()));
		bCarryFloorEnforced = bEnforced;

		// Co-op carry (ADR-0028): a move without the Coop claim after one with it shows the client knows it left.
		if (bServerAcceptClientAuthoritativePosition && ClaimedCarry == EHollowCarryClaim::Coop)
		{
			bSawCoopClaim = true;
		}
		if (CoopReturn.IsSet() && !CoopReturn->KnownAt.IsSet() && bSawCoopClaim && ClaimedCarry != EHollowCarryClaim::Coop)
		{
			CoopReturn->KnownAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
			UE_LOG(LogHollowPhysics, Log, TEXT("%s: its moves show it left the co-op carry (claim %s)"), *GetNameSafe(CharacterOwner),
				*HollowEnumToString(ClaimedCarry));
		}
	}
}

void UHollowCharacterMovementComponent::ReplicateMoveToServer(const float DeltaTime, const FVector& NewAcceleration)
{
	// The engine clamps a client move to MaxMoveDeltaTime of simulated time but advances its timestamp by the whole frame,
	// while the server simulates up to p.NetServerMaxMoveDeltaTimeScalar x MaxMoveDeltaTime from the timestamps: after a
	// long frame (a hitch, a throttled background client) the server runs ahead and corrects. That was the first-step
	// correction of ADR-0015 (server ahead by (frame - 0.125 s) x speed). Several whole moves simulate the same time on
	// both sides (ADR-0028).
	const FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	const float MaxStep = ClientData && CharacterOwner ? ClientData->MaxMoveDeltaTime * CharacterOwner->GetActorTimeDilation() : 0.f;
	const int32 Steps = HollowMovementMath::MoveSteps(DeltaTime, MaxStep);
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		Super::ReplicateMoveToServer(DeltaTime / Steps, NewAcceleration);
	}
}

void UHollowCharacterMovementComponent::MoveAutonomous(const float ClientTimeStamp, const float DeltaTime, const uint8 CompressedFlags, const FVector& NewAccel)
{
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
	if (UpdatedComponent)
	{
		SimulatedLocation = UpdatedComponent->GetComponentLocation();
		bHasSimulatedLocation = true;
	}
}

void UHollowCharacterMovementComponent::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	bHasSimulatedLocation = false;
	Super::ServerMove_PerformMovement(MoveData);
	const FNetworkPredictionData_Server_Character* ServerData = HasPredictionData_Server() ? GetPredictionData_Server_Character() : nullptr;
	if (!ServerData || !bHasSimulatedLocation || !UpdatedComponent || ServerData->PendingAdjustment.TimeStamp != MoveData.TimeStamp)
	{
		return;   // not simulated, or not checked (the engine checks a client's moves at a limited rate)
	}
	// After a checked move the server stands where its client said: the error is how far its own simulation was off.
	const FVector Error = SimulatedLocation - UpdatedComponent->GetComponentLocation();
	const bool bInSync = !GetDefault<AGameNetworkManager>()->ExceedsAllowablePositionError(Error);
	if (bServerAcceptClientAuthoritativePosition)
	{
		AcceptedClientLocation = UpdatedComponent->GetComponentLocation();   // the client's claim (the sanity check's data)
		AcceptedClientTimeStamp = MoveData.TimeStamp;
		bHasAcceptedClientMove = true;
		UpdateCoopReturn(bInSync);
	}
#if !UE_BUILD_SHIPPING
	// ADR-0015 investigation: details of every correction a carrier gets.
	if (!ServerData->PendingAdjustment.bAckGoodMove && (ClaimedCarry != EHollowCarryClaim::None || ServerCarry.Current != EHollowCarryClaim::None))
	{
		const FVector ClientLocation = FRepMovement::RebaseOntoLocalOrigin(MoveData.Location, this);
		UE_LOG(LogHollowPhysics, Log, TEXT("%s: carrier corrected at t=%.3f dt=%.4f (%s move): claim %s, effective %s, server %s, accel %s, velocity %s, off %.2f cm"),
			*GetNameSafe(CharacterOwner), MoveData.TimeStamp, ServerData->PendingAdjustment.DeltaTime,
			MoveData.NetworkMoveType == FCharacterNetworkMoveData::ENetworkMoveType::NewMove ? TEXT("new")
				: MoveData.NetworkMoveType == FCharacterNetworkMoveData::ENetworkMoveType::PendingMove ? TEXT("pending") : TEXT("old"),
			*HollowEnumToString(ClaimedCarry), *HollowEnumToString(GetEffectiveCarry()), *HollowEnumToString(ServerCarry.Current),
			*Acceleration.ToCompactString(), *Velocity.ToCompactString(), FVector::Dist(SimulatedLocation, ClientLocation));
	}
#endif
}

bool UHollowCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	// Replaying saved moves decodes their flags; keep the live requests like the engine does for crouch/jump.
	++CorrectionCount;   // the engine replays saved moves only after a correction (bUpdatePosition)
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
