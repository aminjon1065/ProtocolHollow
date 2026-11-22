#include "World/HollowRitualAltar.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "HollowGameLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Threat/HollowAIPlayers.h"
#include "Threat/HollowActivityHeatmapSubsystem.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"
#include "World/HollowRitualAltarDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRitualAltar)

namespace HollowRitualAltarPrivate
{
	/** A toll whose time passed less than this before the state arrived still sounds (replication delay). */
	constexpr double LateTollSeconds = 0.25;
}

AHollowRitualAltar::AHollowRitualAltar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);   // it stands where the sector put it
	NetDormancy = DORM_DormantAll;   // nothing to send between its few changes (ADR-0002)

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Pieces = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Pieces"));
	Pieces->SetupAttachment(Root);
	Pieces->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);   // solid; the interaction probe hits it

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);
}

void AHollowRitualAltar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AHollowRitualAltar, AltarState, Params);
}

void AHollowRitualAltar::BeginPlay()
{
	Super::BeginPlay();
	if (!Definition)
	{
		UE_LOG(LogHollowGame, Error, TEXT("%s has no altar definition."), *GetName());
		return;
	}
	for (const FTransform& Piece : Definition->Pieces)
	{
		Pieces->AddInstance(Piece);
	}
	GlowIntensity = Glow->Intensity;
	ApplyState();   // a state replicated before BeginPlay (a late joiner)
}

void AHollowRitualAltar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())   // null when the garbage collector ends play
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

double AHollowRitualAltar::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

float AHollowRitualAltar::GetSecondsLeft() const
{
	if (!Definition || AltarState.Phase != EHollowAltarPhase::Ritual)
	{
		return 0.f;
	}
	return FMath::Max(static_cast<float>(Definition->RitualSeconds - (ServerNow() - AltarState.RitualStartServerTime)), 0.f);
}

FTransform AHollowRitualAltar::GetSeatTransform() const
{
	return Definition ? Definition->Seat * GetActorTransform() : GetActorTransform();
}

UHollowCarryableComponent* AHollowRitualAltar::CarriedBy(const AActor* Interactor) const
{
	const UHollowCarrierComponent* Carrier = Interactor ? Interactor->FindComponentByClass<UHollowCarrierComponent>() : nullptr;
	return Carrier ? Carrier->GetCarried() : nullptr;
}

FHollowInteractionInfo AHollowRitualAltar::GetInteractionInfo(const AActor* Interactor) const
{
	FHollowInteractionInfo Info;
	if (Definition)
	{
		Info.Prompt = Definition->PlacePrompt;
	}
	return Info;
}

bool AHollowRitualAltar::CanInteract(const AActor* Interactor) const
{
	// Only to hand over what it purifies: a carried object that counts as the accepted item.
	const UHollowCarryableComponent* Carryable = CarriedBy(Interactor);
	return Definition && AltarState.Phase == EHollowAltarPhase::Idle && Carryable && !Carryable->IsHeld()
		&& Carryable->GetLootItem() == Definition->AcceptedItem;
}

void AHollowRitualAltar::Interact(AActor* Interactor)
{
	UHollowCarryableComponent* Carryable = CarriedBy(Interactor);
	if (!HasAuthority() || !CanInteract(Interactor) || !Carryable->PlaceAt(GetSeatTransform(), Interactor))
	{
		return;
	}
	Carryable->SetHeld(true);   // every carrier let go; nobody lifts it until the ritual is over
	Starter = Cast<APawn>(Interactor);
	Tolls = 0;

	FHollowAltarNetState NewState;
	NewState.Phase = EHollowAltarPhase::Ritual;
	NewState.RitualStartServerTime = ServerNow();
	NewState.Held = Carryable->GetOwner();
	GetWorldTimerManager().SetTimer(CompleteTimer, this, &ThisClass::Complete, Definition->RitualSeconds, /*bLoop=*/false);

	// The Echo-Stalker feels the ritual at once (ADR-0019 heat), before the bell has said anything.
	const UHollowSectorTopologySubsystem* Topology = UHollowSectorTopologySubsystem::Get(this);
	UHollowActivityHeatmapSubsystem* Heatmap = UHollowActivityHeatmapSubsystem::Get(this);
	const int32 Room = Topology ? Topology->GetTopology().RoomAt(GetActorLocation()) : INDEX_NONE;
	if (Heatmap && Room != INDEX_NONE)
	{
		Heatmap->AddRoomHeat(Room, Definition->HeatBurst);
	}
	UE_LOG(LogHollowGame, Log, TEXT("%s: %s lays %s on the altar; the ritual takes %.0f s (room %d +%.0f heat)"), *GetName(), *GetNameSafe(Interactor),
		*GetNameSafe(NewState.Held), Definition->RitualSeconds, Room, Definition->HeatBurst);
	SetAltarState(NewState);   // the first toll sounds now
}

void AHollowRitualAltar::Complete()
{
	UHollowCarryableComponent* Carryable = AltarState.Held ? AltarState.Held->FindComponentByClass<UHollowCarryableComponent>() : nullptr;
	if (!HasAuthority() || !Definition || AltarState.Phase != EHollowAltarPhase::Ritual)
	{
		return;
	}
	if (Carryable)
	{
		Carryable->SetLootItemOverride(Definition->ResultItem);
		Carryable->SetHeld(false);
	}
	FHollowAltarNetState NewState = AltarState;
	NewState.Phase = EHollowAltarPhase::Spent;
	UE_LOG(LogHollowGame, Log, TEXT("%s: the ritual is over after %d toll(s): %s counts as %s"), *GetName(), Tolls, *GetNameSafe(AltarState.Held),
		Carryable ? *Carryable->GetLootItem().ToString() : TEXT("nothing (it is gone)"));
	SetAltarState(NewState);
}

void AHollowRitualAltar::SetAltarState(const FHollowAltarNetState& NewState)
{
	AltarState = NewState;
	FlushNetDormancy();
	MARK_PROPERTY_DIRTY_FROM_NAME(AHollowRitualAltar, AltarState, this);
	ApplyState();
	ForceNetUpdate();
}

void AHollowRitualAltar::OnRep_AltarState()
{
	UE_LOG(LogHollowGame, Log, TEXT("%s: %s (replicated)"), *GetName(), *Describe());
	if (HasActorBegunPlay())
	{
		ApplyState();
	}
}

void AHollowRitualAltar::ApplyState()
{
	using namespace HollowRitualAltarPrivate;
	if (!Definition)
	{
		return;
	}
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(TollTimer);
	Timers.ClearTimer(FlashTimer);
	const bool bJustPurified = ShownPhase == EHollowAltarPhase::Ritual && AltarState.Phase == EHollowAltarPhase::Spent;
	ShownPhase = AltarState.Phase;
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this);
	if (bJustPurified && Audio && Definition->PurifiedSound.IsValid())
	{
		Audio->Play(Definition->PurifiedSound, GetSeatTransform().GetLocation());   // S7-03: heard where it was seen
	}
	switch (AltarState.Phase)
	{
	case EHollowAltarPhase::Idle:
		Glow->SetVisibility(false);
		return;
	case EHollowAltarPhase::Spent:
		Glow->SetIntensity(GlowIntensity * Definition->SpentGlow);
		Glow->SetVisibility(true);
		return;
	case EHollowAltarPhase::Ritual:
		break;
	}
	Glow->SetIntensity(GlowIntensity * Definition->RitualGlow);
	Glow->SetVisibility(true);
	// Tolls at 0, PulseSeconds, 2 x PulseSeconds... within the ritual, on the server's clock: catch up with it.
	const double Elapsed = ServerNow() - AltarState.RitualStartServerTime;
	NextToll = FMath::Max(FMath::CeilToInt32((Elapsed - LateTollSeconds) / Definition->PulseSeconds), 0);
	const double At = NextToll * static_cast<double>(Definition->PulseSeconds);
	if (At >= Definition->RitualSeconds)
	{
		return;
	}
	if (At <= Elapsed)
	{
		Toll();
	}
	else
	{
		Timers.SetTimer(TollTimer, this, &ThisClass::Toll, static_cast<float>(At - Elapsed), /*bLoop=*/false);
	}
}

void AHollowRitualAltar::Toll()
{
	if (!Definition || AltarState.Phase != EHollowAltarPhase::Ritual)
	{
		return;
	}
	const FVector Bowl = GetSeatTransform().GetLocation();
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
	{
		Audio->Play(Definition->NoiseTag, Bowl);   // the bell the AI hears, as far as it hears it (ADR-0022)
	}
	Glow->SetIntensity(GlowIntensity);
	GetWorldTimerManager().SetTimer(FlashTimer, this, &ThisClass::EndFlash, FMath::Max(Definition->FlashSeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
	if (HasAuthority())
	{
		ReportToll();
	}
	++NextToll;
	const double Elapsed = ServerNow() - AltarState.RitualStartServerTime;
	const double At = NextToll * static_cast<double>(Definition->PulseSeconds);
	if (At < Definition->RitualSeconds)
	{
		GetWorldTimerManager().SetTimer(TollTimer, this, &ThisClass::Toll, static_cast<float>(FMath::Max(At - Elapsed, UE_KINDA_SMALL_NUMBER)), /*bLoop=*/false);
	}
}

void AHollowRitualAltar::EndFlash()
{
	if (Definition && AltarState.Phase == EHollowAltarPhase::Ritual)
	{
		Glow->SetIntensity(GlowIntensity * Definition->RitualGlow);
	}
}

void AHollowRitualAltar::ReportToll()
{
	// The AI hears only the living players (ADR-0018): the toll is the squad's, first the one who laid the core.
	APawn* Instigator = HollowAIPlayers::IsLivingPlayer(Starter.Get()) ? Starter.Get() : nullptr;
	if (!Instigator)
	{
		double Nearest = TNumericLimits<double>::Max();
		HollowAIPlayers::ForEach(GetWorld(), [this, &Instigator, &Nearest](APawn& Player)
		{
			const double Distance = FVector::DistSquared(Player.GetActorLocation(), GetActorLocation());
			if (Distance < Nearest)
			{
				Nearest = Distance;
				Instigator = &Player;
			}
		});
	}
	++Tolls;
	UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(this);
	const bool bHeard = Instigator && Noise && Noise->ReportNoise(FHollowNoiseEvent(Definition->NoiseTag, GetSeatTransform().GetLocation(), Instigator));
	UE_LOG(LogHollowGame, Log, TEXT("%s: toll %d, %s by %s"), *GetName(), Tolls, bHeard ? TEXT("heard") : TEXT("not reported"), *GetNameSafe(Instigator));
}

FString AHollowRitualAltar::Describe() const
{
	const UHollowCarryableComponent* Carryable = AltarState.Held ? AltarState.Held->FindComponentByClass<UHollowCarryableComponent>() : nullptr;
	return FString::Printf(TEXT("%s at %s, %s, %.1f s left, holds %s (%s%s), tolls %d"), *GetName(), *GetActorLocation().ToCompactString(),
		*HollowEnumToString(AltarState.Phase), GetSecondsLeft(), *GetNameSafe(AltarState.Held), Carryable ? *Carryable->GetLootItem().ToString() : TEXT("-"),
		Carryable && Carryable->IsHeld() ? TEXT(", held") : TEXT(""), HasAuthority() ? Tolls : NextToll);
}
