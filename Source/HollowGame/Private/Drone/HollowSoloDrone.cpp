#include "Drone/HollowSoloDrone.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Drone/HollowDroneDefinition.h"
#include "GameFramework/SpringArmComponent.h"
#include "HollowGameLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Tags/HollowGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSoloDrone)

AHollowSoloDrone::AHollowSoloDrone(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;   // the arm (an engine component) moves it; everything else is events (CLAUDE.md sec. 1)
	bReplicates = true;
	SetReplicatingMovement(false);           // rides its player's capsule on every machine (BindToPlayer)

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Root);
	Arm->bDoCollisionTest = true;            // walls push it in towards the player
	// The probe starts inside its player's capsule: Visibility, which walls block and pawns, carried cores and debris
	// ignore (Camera, the arm's default, is blocked by the Pawn profile).
	Arm->ProbeChannel = ECC_Visibility;
	Arm->bEnableCameraLag = true;
	Arm->bEnableCameraRotationLag = true;
	Arm->bUsePawnControlRotation = false;
	Arm->SetUsingAbsoluteRotation(true);     // its own heading (UpdateHeading), level when the player looks up or down

	Hover = CreateDefaultSubobject<USceneComponent>(TEXT("Hover"));
	Hover->SetupAttachment(Arm, USpringArmComponent::SocketName);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Hover);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // no pawn, shot or probe ever meets it
	Mesh->SetGenerateOverlapEvents(false);

	Focus = CreateDefaultSubobject<USphereComponent>(TEXT("Focus"));
	Focus->SetupAttachment(Hover);
	Focus->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Focus->SetCollisionObjectType(ECC_WorldDynamic);
	Focus->SetCollisionResponseToAllChannels(ECR_Ignore);        // RefreshFocusable opens the Interaction channel
	Focus->SetGenerateOverlapEvents(false);
	Focus->SetCanEverAffectNavigation(false);

	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Hover);
	Light->SetCastShadows(false);

	CarryPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CarryPoint"));
	CarryPoint->SetupAttachment(Hover);
}

void AHollowSoloDrone::SetPlayer(AHollowExoFrameCharacter* InPlayer)
{
	if (!HasAuthority() || Player == InPlayer)
	{
		return;
	}
	Player = InPlayer;
	MARK_PROPERTY_DIRTY_FROM_NAME(AHollowSoloDrone, Player, this);
	if (HasActorBegunPlay())
	{
		BindToPlayer();
	}
}

void AHollowSoloDrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowSoloDrone, Player, Params);
}

void AHollowSoloDrone::BeginPlay()
{
	Super::BeginPlay();
	ApplyDefinition();
	BindToPlayer();
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
	{
		Hum = Audio->PlayAttached(HollowTags::Sound_Drone_Hum, Hover);   // ends with the drone
	}
	UE_LOG(LogHollowGame, Log, TEXT("%s: serving %s"), *GetName(), *GetNameSafe(Player));
}

void AHollowSoloDrone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Going away with its player's core (the raid ends, the player leaves): the player holds it again.
	UHollowCarryableComponent* Carryable = GetCarried();
	if (HasAuthority() && Carryable && EndPlayReason == EEndPlayReason::Destroyed && IsValid(Player))
	{
		Carryable->EndDrone(Player);
	}
	UnbindFromPlayer();
	Super::EndPlay(EndPlayReason);
}

void AHollowSoloDrone::OnRep_Player()
{
	if (HasActorBegunPlay())
	{
		BindToPlayer();
	}
}

void AHollowSoloDrone::ApplyDefinition()
{
	if (!Definition)
	{
		UE_LOG(LogHollowGame, Error, TEXT("%s: a solo drone needs a definition (DA_SoloDrone)."), *GetName());
		return;
	}
	// The arm points from the hover spot back to the player, so its probe sweeps the line between them (an arm of
	// length 0 never probes); the hover frame at its end turns back to the player's facing.
	Back = (-Definition->FollowOffset).Rotation();
	Arm->TargetArmLength = static_cast<float>(Definition->FollowOffset.Size());
	Arm->SocketOffset = FVector::ZeroVector;
	Arm->CameraLagSpeed = Definition->FollowLagSpeed;
	Arm->CameraRotationLagSpeed = Definition->TurnLagSpeed;
	Arm->CameraLagMaxDistance = Definition->MaxLagDistance;
	Arm->ProbeSize = Definition->ProbeRadius;
	Hover->SetRelativeRotation(Back.Quaternion().Inverse());
	Focus->SetSphereRadius(Definition->FocusRadius);
	CarryPoint->SetRelativeLocation(Definition->CarryPointOffset);
}

void AHollowSoloDrone::BindToPlayer()
{
	if (BoundPlayer.Get() == Player)
	{
		return;
	}
	UnbindFromPlayer();
	if (!IsValid(Player))
	{
		return;
	}
	BoundPlayer = Player;
	SetOwner(Player);
	SetInstigator(Player);
	AttachToActor(Player, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	if (UHollowCarrierComponent* Carrier = Player->GetCarrierComponent())
	{
		CarriedHandle = Carrier->OnCarriedChanged.AddUObject(this, &ThisClass::HandleCarriedChanged);
	}
	DiedHandle = Player->OnDied.AddUObject(this, &ThisClass::HandlePlayerDied);
	Heading.Reset();
	UpdateHeading();
	if (Definition)
	{
		GetWorldTimerManager().SetTimer(HeadingTimer, this, &ThisClass::UpdateHeading, 1.f / FMath::Max(Definition->HeadingRateHz, 1.f), /*bLoop=*/true);
	}
	if (HasAuthority())
	{
		Player->OnEndPlay.AddDynamic(this, &ThisClass::HandlePlayerEndPlay);
	}
	if (Player->IsDead())
	{
		GoDark();
	}
	RefreshFocusable();
}

void AHollowSoloDrone::UnbindFromPlayer()
{
	AHollowExoFrameCharacter* Bound = BoundPlayer.Get();
	BoundPlayer = nullptr;
	GetWorldTimerManager().ClearTimer(HeadingTimer);
	if (!Bound)
	{
		return;
	}
	if (UHollowCarrierComponent* Carrier = Bound->GetCarrierComponent())
	{
		Carrier->OnCarriedChanged.Remove(CarriedHandle);
	}
	Bound->OnDied.Remove(DiedHandle);
	Bound->OnEndPlay.RemoveDynamic(this, &ThisClass::HandlePlayerEndPlay);
	CarriedHandle.Reset();
	DiedHandle.Reset();
}

void AHollowSoloDrone::HandleCarriedChanged(const UHollowCarrierComponent& /*Carrier*/)
{
	RefreshFocusable();
}

void AHollowSoloDrone::HandlePlayerDied(AHollowExoFrameCharacter& /*Character*/)
{
	GoDark();
}

void AHollowSoloDrone::HandlePlayerEndPlay(AActor* Actor, const EEndPlayReason::Type Reason)
{
	if (HasAuthority() && Reason == EEndPlayReason::Destroyed)
	{
		UE_LOG(LogHollowGame, Log, TEXT("%s: %s is gone, so is its drone"), *GetName(), *GetNameSafe(Actor));
		Destroy();
	}
}

void AHollowSoloDrone::GoDark()
{
	if (bDark)
	{
		return;
	}
	bDark = true;
	Light->SetVisibility(false);
	if (Hum)
	{
		Hum->Stop();
	}
	RefreshFocusable();
	UE_LOG(LogHollowGame, Log, TEXT("%s: %s died, the drone goes dark"), *GetName(), *GetNameSafe(Player));
}

void AHollowSoloDrone::UpdateHeading()
{
	if (!Definition || !IsValid(Player))
	{
		return;
	}
	const double ViewYaw = Player->GetBaseAimRotation().Yaw;
	if (Heading.IsSet() && FMath::Abs(FMath::FindDeltaAngleDegrees(Heading.GetValue(), ViewYaw)) <= Definition->HeadingDeadZone)
	{
		return;
	}
	Heading = ViewYaw;
	Arm->SetWorldRotation(FRotator(Back.Pitch, ViewYaw + Back.Yaw, Back.Roll));   // the arm's rotation lag swings it round
}

void AHollowSoloDrone::RefreshFocusable()
{
	Focus->SetCollisionResponseToChannel(HollowCollision::Interaction, IsFocusable() ? ECR_Block : ECR_Ignore);
}

bool AHollowSoloDrone::IsFocusable() const
{
	return IsValid(Player) && CanInteract(Player);
}

UHollowCarryableComponent* AHollowSoloDrone::CarriedBy(const AActor* Interactor) const
{
	const UHollowCarrierComponent* Carrier = IsValid(Player) && Interactor == Player ? Player->GetCarrierComponent() : nullptr;
	return Carrier ? Carrier->GetCarried() : nullptr;
}

bool AHollowSoloDrone::CanTakeBackFrom(const UHollowCarryableComponent& Carryable, const APawn* Pawn) const
{
	return Carryable.GetCarryState().Vehicle == this && Carryable.CanTakeBack(Pawn);
}

FHollowInteractionInfo AHollowSoloDrone::GetInteractionInfo(const AActor* Interactor) const
{
	FHollowInteractionInfo Info;
	const UHollowCarryableComponent* Carryable = CarriedBy(Interactor);
	if (Definition && Carryable)
	{
		Info.Prompt = CanTakeBackFrom(*Carryable, Cast<APawn>(Interactor)) ? Definition->TakeBackPrompt : Definition->HandOverPrompt;
	}
	return Info;
}

bool AHollowSoloDrone::CanInteract(const AActor* Interactor) const
{
	const UHollowCarryableComponent* Carryable = CarriedBy(Interactor);
	const APawn* Pawn = Cast<APawn>(Interactor);
	return !bDark && Definition && Carryable && !Player->IsDead() && (Carryable->CanHandTo(Pawn, this) || CanTakeBackFrom(*Carryable, Pawn));
}

void AHollowSoloDrone::Interact(AActor* Interactor)
{
	UHollowCarryableComponent* Carryable = CarriedBy(Interactor);
	if (!HasAuthority() || !Carryable || !CanInteract(Interactor))
	{
		return;
	}
	if (CanTakeBackFrom(*Carryable, Player))
	{
		Carryable->EndDrone(Player);
	}
	else
	{
		Carryable->BeginDrone(Player, this);
	}
}

USceneComponent* AHollowSoloDrone::GetCarryPoint() const
{
	return CarryPoint;
}

APawn* AHollowSoloDrone::GetCarryOwner() const
{
	return Player;
}

UHollowCarryableComponent* AHollowSoloDrone::GetCarried() const
{
	UHollowCarryableComponent* Carryable = CarriedBy(Player);
	return Carryable && Carryable->GetCarryState().Mode == EHollowCarryMode::Drone && Carryable->GetCarryState().Vehicle == this ? Carryable : nullptr;
}

bool AHollowSoloDrone::IsLit() const
{
	return !bDark && Light->IsVisible() && Light->Intensity > 0.f;
}

bool AHollowSoloDrone::IsHumming() const
{
	return Hum && Hum->IsPlaying();
}

FString AHollowSoloDrone::Describe() const
{
	const UHollowCarryableComponent* Carryable = GetCarried();
	FVector Eyes = FVector::ZeroVector;
	FRotator Unused;
	if (IsValid(Player))
	{
		Player->GetActorEyesViewPoint(Eyes, Unused);
	}
	return FString::Printf(TEXT("%s serves %s: %s, %s, carrying %s, focusable %s, %.0f cm from the eyes, pushed in %.0f cm by walls"), *GetName(),
		*GetNameSafe(Player), IsLit() ? TEXT("lit") : TEXT("dark"), IsHumming() ? TEXT("humming") : TEXT("silent"),
		Carryable ? *GetNameSafe(Carryable->GetOwner()) : TEXT("nothing"), IsFocusable() ? TEXT("yes") : TEXT("no"),
		IsValid(Player) ? FVector::Dist(Eyes, Hover->GetComponentLocation()) : 0.0,
		Arm->GetUnfixedCameraPosition().IsZero() ? 0.0 : FVector::Dist(Arm->GetUnfixedCameraPosition(), Hover->GetComponentLocation()));
}
