#include "Raid/HollowRaidFlowComponent.h"

#include "Assembly/HollowSectorStateComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Classes/HollowClassComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/HollowPlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HollowGameLog.h"
#include "Crafting/HollowCraftingRules.h"
#include "Crafting/HollowShipUpgradeDefinition.h"
#include "Inventory/HollowInventoryComponent.h"
#include "Inventory/HollowLoadoutRules.h"
#include "Lore/HollowLoreSubsystem.h"
#include "Profile/HollowEconomySettings.h"
#include "Raid/HollowExtractionZone.h"
#include "Raid/HollowRaidLauncher.h"
#include "Raid/HollowRaidRules.h"
#include "Raid/HollowRaidSettings.h"
#include "Raid/HollowRaidStateComponent.h"
#include "Session/HollowNetworkSettings.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRaidFlowComponent)

UHollowRaidFlowComponent::UHollowRaidFlowComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;   // timers only (CLAUDE.md sec. 1)
}

void UHollowRaidFlowComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	RaidId = FGuid::NewGuid();
	StartTime = GetWorld()->GetTimeSeconds();
	UpdateState(EHollowRaidPhase::Assembling, 0.f, 0, 0);
	if (UHollowSectorStateComponent* Sector = UHollowSectorStateComponent::Get(GetWorld()))
	{
		ReadyHandle = Sector->OnSectorReadyChanged.AddUObject(this, &ThisClass::HandleSectorReady);
		if (Sector->IsSectorReady())
		{
			HandleSectorReady(true);
		}
	}
	UE_LOG(LogHollowGame, Log, TEXT("Raid %s: waiting for the sector (contract %s)"), *RaidId.ToString(), Contract.IsValid() ? *Contract.ToString() : TEXT("none"));
	// Players who arrived before the raid had its id (the host's own after seamless travel) are asked now.
	for (const TWeakObjectPtr<AHollowPlayerController>& Player : Arrived)
	{
		RequestLoadout(Player.Get());
	}
}

void UHollowRaidFlowComponent::HandlePlayerArrived(AHollowPlayerController* Player)
{
	if (Player)
	{
		Arrived.AddUnique(Player);
		RequestLoadout(Player);
	}
}

void UHollowRaidFlowComponent::RequestLoadout(AHollowPlayerController* Player)
{
	if (!Player || !RaidId.IsValid() || LoadoutsTaken.Contains(Player))
	{
		return;
	}
	UE_LOG(LogHollowGame, Log, TEXT("Raid %s: asks %s for the loadout"), *RaidId.ToString(), *Player->GetName());
	Player->Client_RequestLoadout(RaidId);
}

bool UHollowRaidFlowComponent::AcceptLoadout(AController* Player, const FHollowLoadoutSnapshot& Loadout)
{
	const UHollowRaidStateComponent* StateComponent = GetStateComponent();
	const EHollowRaidPhase Phase = StateComponent ? StateComponent->GetState().Phase : EHollowRaidPhase::Assembling;
	UHollowInventoryComponent* Inventory = Player ? UHollowInventoryComponent::Get(Player->PlayerState) : nullptr;
	const TCHAR* Refused = !GetOwner()->HasAuthority() ? TEXT("not the host")
		: !Inventory ? TEXT("no inventory")
		: (!RaidId.IsValid() || Loadout.RaidId != RaidId) ? TEXT("for another raid")
		: (Phase != EHollowRaidPhase::Assembling && Phase != EHollowRaidPhase::Deploying) ? TEXT("the raid has begun")
		: LoadoutsTaken.Contains(Player) ? TEXT("already taken")
		: nullptr;
	if (Refused)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("Raid %s: loadout of %s for raid %s ignored: %s"), *RaidId.ToString(), *GetNameSafe(Player),
			*Loadout.RaidId.ToString(), Refused);
		return false;
	}
	TArray<FHollowLoadoutIssue> Issues;
	const TArray<FHollowItemStack> Accepted = HollowLoadoutRules::ValidateForRaid(Loadout.Slots, Loadout.Upgrades, &Issues);
	UE_CLOG(!Issues.IsEmpty(), LogHollowGame, Warning, TEXT("Raid %s: loadout of %s cut down: %s"), *RaidId.ToString(), *Player->GetName(),
		*HollowLoadoutRules::Describe(Issues));
	LoadoutsTaken.Add(Player);
	// The ship's upgrades the player owns give their items on top, free (ADR-0032); unknown ids give nothing.
	const FHollowUpgradeEffects Ship = HollowCraftingRules::SumEffects(Loadout.Upgrades, UHollowShipUpgradeDefinition::LoadAllById());
	Inventory->ResetFromLoadout(Accepted, Ship.RaidItems);
	// The class the player plays (ADR-0034), trusted like the loadout; none named: the default, one this host does not
	// know: the default too (said so).
	const FPrimaryAssetId& DefaultClass = GetDefault<UHollowEconomySettings>()->DefaultClass;
	UHollowClassComponent* Class = UHollowClassComponent::Get(Player->PlayerState);
	if (Class && !Class->SetClass(Loadout.ClassId.IsValid() ? Loadout.ClassId : DefaultClass))
	{
		Class->SetClass(DefaultClass);
	}
	// NAR-01b: which crew logs this player has found, so the squad's datapads give the ones the fewest of them know.
	if (UHollowLoreSubsystem* Lore = UHollowLoreSubsystem::Get(GetWorld()))
	{
		Lore->SetKnownLore(Player, Loadout.KnownLore);
	}
	UE_LOG(LogHollowGame, Log, TEXT("Raid %s: %s as %s takes %s (the ship adds %s)"), *RaidId.ToString(), *Player->GetName(),
		Class ? *Class->Describe() : TEXT("no class"), *Inventory->Describe(), *HollowLoadoutRules::Describe(Ship.RaidItems));
	return true;
}

void UHollowRaidFlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHollowSectorStateComponent* Sector = UHollowSectorStateComponent::Get(GetWorld()))
	{
		Sector->OnSectorReadyChanged.Remove(ReadyHandle);
	}
	// Also reached from garbage collection of a torn-down world (a component's BeginDestroy ends play): the world may be gone.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
		World->GetTimerManager().ClearTimer(PollTimer);
	}
	Super::EndPlay(EndPlayReason);
}

UHollowRaidStateComponent* UHollowRaidFlowComponent::GetStateComponent() const
{
	return UHollowRaidStateComponent::Get(GetWorld());
}

void UHollowRaidFlowComponent::UpdateState(const EHollowRaidPhase Phase, const float PhaseEndServerTime, const int32 LivingInZone, const int32 CoresInZone)
{
	UHollowRaidStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent)
	{
		return;
	}
	FHollowRaidNetState State;
	State.Phase = Phase;
	State.RaidId = RaidId;
	State.PhaseEndServerTime = PhaseEndServerTime;
	State.LivingPlayers = static_cast<uint8>(FMath::Clamp(GetLivingPlayers().Num(), 0, 255));
	State.LivingInZone = static_cast<uint8>(FMath::Clamp(LivingInZone, 0, 255));
	State.CoresInZone = static_cast<uint8>(FMath::Clamp(CoresInZone, 0, 255));
	StateComponent->SetState(State);
}

void UHollowRaidFlowComponent::HandleSectorReady(const bool bReady)
{
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	const UHollowRaidStateComponent* StateComponent = GetStateComponent();
	const EHollowRaidPhase Phase = StateComponent ? StateComponent->GetState().Phase : EHollowRaidPhase::Assembling;
	if (!bReady)
	{
		// A rebuild (development): back to assembling, the same raid.
		Timers.ClearTimer(PhaseTimer);
		Timers.ClearTimer(PollTimer);
		if (Phase != EHollowRaidPhase::Debrief)
		{
			UpdateState(EHollowRaidPhase::Assembling, 0.f, 0, 0);
		}
		return;
	}
	if (Phase != EHollowRaidPhase::Assembling || !StateComponent)
	{
		return;
	}
	const float DeploySeconds = GetDefault<UHollowRaidSettings>()->DeploySeconds;
	UpdateState(EHollowRaidPhase::Deploying, static_cast<float>(StateComponent->GetServerTime() + DeploySeconds), 0, 0);
	Timers.SetTimer(PhaseTimer, this, &ThisClass::StartRaid, FMath::Max(DeploySeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

void UHollowRaidFlowComponent::StartRaid()
{
	UpdateState(EHollowRaidPhase::InRaid, 0.f, 0, 0);
	const float RateHz = GetDefault<UHollowRaidSettings>()->ZonePollRateHz;
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &ThisClass::PollZone, 1.f / FMath::Max(RateHz, 1.f), /*bLoop=*/true);
	UE_LOG(LogHollowGame, Log, TEXT("Raid %s: in raid with %d player(s)"), *RaidId.ToString(), GetLivingPlayers().Num());
	UE_CLOG(!FindZone(), LogHollowGame, Warning, TEXT("Raid %s: the sector has no extraction zone; only deaths can end it"), *RaidId.ToString());
}

AHollowExtractionZone* UHollowRaidFlowComponent::FindZone()
{
	if (!Zone.IsValid())
	{
		TActorIterator<AHollowExtractionZone> It(GetWorld());
		Zone = It ? *It : nullptr;
	}
	return Zone.Get();
}

TArray<AController*> UHollowRaidFlowComponent::GetLivingPlayers() const
{
	TArray<AController*> Living;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (Player && Player->GetPawn() && GetOutcome(Player) == EHollowRaidOutcome::None)
		{
			Living.Add(Player);
		}
	}
	return Living;
}

EHollowRaidOutcome UHollowRaidFlowComponent::GetOutcome(const AController* Player) const
{
	const EHollowRaidOutcome* Outcome = Outcomes.Find(MakeWeakObjectPtr(const_cast<AController*>(Player)));
	return Outcome ? *Outcome : EHollowRaidOutcome::None;
}

void UHollowRaidFlowComponent::PollZone()
{
	const UHollowRaidStateComponent* StateComponent = GetStateComponent();
	AHollowExtractionZone* ExtractionZone = FindZone();
	if (!StateComponent || !ExtractionZone)
	{
		return;
	}
	TArray<APawn*> Pawns;
	TArray<UHollowCarryableComponent*> Carryables;
	ExtractionZone->Sample(Pawns, Carryables);
	int32 LivingInZone = 0;
	for (const AController* Player : GetLivingPlayers())
	{
		LivingInZone += Pawns.Contains(Player->GetPawn()) ? 1 : 0;
	}

	const FHollowRaidNetState& State = StateComponent->GetState();
	const bool bCounting = State.Phase == EHollowRaidPhase::Extraction;
	const double Now = StateComponent->GetServerTime();
	switch (HollowRaidRules::StepCountdown(bCounting, Now, State.PhaseEndServerTime, LivingInZone))
	{
	case EHollowCountdownStep::Start:
		UE_LOG(LogHollowGame, Log, TEXT("Raid %s: shuttle departs in %.0f s (%d in the zone)"), *RaidId.ToString(),
			GetDefault<UHollowRaidSettings>()->ExtractionCountdownSeconds, LivingInZone);
		UpdateState(EHollowRaidPhase::Extraction, static_cast<float>(Now + GetDefault<UHollowRaidSettings>()->ExtractionCountdownSeconds),
			LivingInZone, Carryables.Num());
		break;
	case EHollowCountdownStep::Reset:
		UE_LOG(LogHollowGame, Log, TEXT("Raid %s: the zone is empty, departure cancelled"), *RaidId.ToString());
		UpdateState(EHollowRaidPhase::InRaid, 0.f, LivingInZone, Carryables.Num());
		break;
	case EHollowCountdownStep::Depart:
		Depart();
		break;
	case EHollowCountdownStep::None:
		UpdateState(State.Phase, State.PhaseEndServerTime, LivingInZone, Carryables.Num());
		break;
	}
}

void UHollowRaidFlowComponent::Depart()
{
	TArray<APawn*> Pawns;
	TArray<UHollowCarryableComponent*> Carryables;
	if (AHollowExtractionZone* ExtractionZone = FindZone())
	{
		ExtractionZone->Sample(Pawns, Carryables);
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* Player = It->Get())
		{
			const EHollowRaidOutcome Outcome = HollowRaidRules::OutcomeAtDeparture(GetOutcome(Player), Player->GetPawn() && Pawns.Contains(Player->GetPawn()));
			Outcomes.Add(Player, Outcome);
		}
	}
	Loot.Reset();
	for (const UHollowCarryableComponent* Carryable : Carryables)
	{
		// What it counts as now: a purified core sells as one (S7-02).
		const FPrimaryAssetId Item = Carryable->GetLootItem();
		if (!Item.IsValid())
		{
			continue;
		}
		if (FHollowItemStack* Stack = Loot.FindByPredicate([&Item](const FHollowItemStack& S) { return S.Item == Item; }))
		{
			++Stack->Count;
		}
		else
		{
			FHollowItemStack& NewStack = Loot.AddDefaulted_GetRef();
			NewStack.Item = Item;
			NewStack.Count = 1;
		}
	}
	EndRaid(TEXT("the shuttle left"));
}

void UHollowRaidFlowComponent::HandlePlayerDied(AController* Player)
{
	const UHollowRaidStateComponent* StateComponent = GetStateComponent();
	if (!Player || !StateComponent || StateComponent->GetState().Phase == EHollowRaidPhase::Debrief || GetOutcome(Player) != EHollowRaidOutcome::None)
	{
		return;
	}
	Outcomes.Add(Player, EHollowRaidOutcome::Dead);
	UE_LOG(LogHollowGame, Log, TEXT("Raid %s: %s died, %d still alive"), *RaidId.ToString(), *Player->GetName(), GetLivingPlayers().Num());
	if (HollowRaidRules::IsOver(GetLivingPlayers().Num()))
	{
		EndRaid(TEXT("nobody is left alive"));
	}
}

void UHollowRaidFlowComponent::HandlePlayerLeft(AController* Exiting)
{
	const UHollowRaidStateComponent* StateComponent = GetStateComponent();
	if (!Exiting || !StateComponent || StateComponent->GetState().Phase == EHollowRaidPhase::Debrief)
	{
		return;
	}
	if (GetOutcome(Exiting) == EHollowRaidOutcome::None)
	{
		Outcomes.Add(Exiting, EHollowRaidOutcome::Abandoned);
	}
	UE_LOG(LogHollowGame, Log, TEXT("Raid %s: %s left (%s)"), *RaidId.ToString(), *Exiting->GetName(), *HollowEnumToString(GetOutcome(Exiting)));
	const bool bStarted = StateComponent->GetState().Phase != EHollowRaidPhase::Assembling;
	if (bStarted && HollowRaidRules::IsOver(GetLivingPlayers().Num()))
	{
		EndRaid(TEXT("nobody is left in the raid"));
	}
}

void UHollowRaidFlowComponent::EndRaid(const TCHAR* Why)
{
	UHollowRaidStateComponent* StateComponent = GetStateComponent();
	if (!StateComponent || StateComponent->GetState().Phase == EHollowRaidPhase::Debrief)
	{
		return;
	}
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.ClearTimer(PollTimer);
	const UHollowRaidSettings* Settings = GetDefault<UHollowRaidSettings>();
	const float RaidSeconds = static_cast<float>(GetWorld()->GetTimeSeconds() - StartTime);

	int32 SquadSize = 0;
	int32 SquadExtracted = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* Player = It->Get())
		{
			++SquadSize;
			SquadExtracted += GetOutcome(Player) == EHollowRaidOutcome::Extracted ? 1 : 0;
		}
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AHollowPlayerController* Player = Cast<AHollowPlayerController>(It->Get());
		if (!Player)
		{
			continue;
		}
		FHollowRaidResult Result;
		Result.RaidId = RaidId;
		// Still undecided here: the raid ended around them (nobody else left alive) - they were not aboard.
		Result.Outcome = HollowRaidRules::OutcomeAtDeparture(GetOutcome(Player), /*bInZone=*/false);
		Result.Loot = Result.Outcome == EHollowRaidOutcome::Extracted ? Loot : TArray<FHollowItemStack>();
		if (const UHollowInventoryComponent* Inventory = UHollowInventoryComponent::Get(Player->PlayerState))
		{
			Result.Spent = Inventory->GetSpent();
			Result.Found = Inventory->GetFound();   // GP-01b: the player's own, whatever the outcome (its resolver decides)
		}
		Result.RaidSeconds = RaidSeconds;
		Result.SquadExtracted = SquadExtracted;
		Result.SquadSize = SquadSize;
		UE_LOG(LogHollowGame, Log, TEXT("Raid %s: %s -> %s, %d loot stack(s), spent %s, found %s"), *RaidId.ToString(), *Player->GetName(),
			*HollowEnumToString(Result.Outcome), Result.Loot.Num(), *HollowLoadoutRules::Describe(Result.Spent), *HollowLoadoutRules::Describe(Result.Found));
		Player->Client_ReceiveRaidResult(Result);
	}
	UE_LOG(LogHollowGame, Log, TEXT("Raid %s over: %s; %d of %d extracted; back to the hub in %.0f s"), *RaidId.ToString(), Why, SquadExtracted,
		SquadSize, Settings->DebriefSeconds);
	UpdateState(EHollowRaidPhase::Debrief, static_cast<float>(StateComponent->GetServerTime() + Settings->DebriefSeconds), 0, 0);
	Timers.SetTimer(PhaseTimer, this, &ThisClass::ReturnToHub, FMath::Max(Settings->DebriefSeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

void UHollowRaidFlowComponent::ReturnToHub()
{
	// The hub keeps the squad's contract (ADR-0041): the host picks again only to change it.
	const FString Hub = HollowRaidLauncher::MakeHubUrl(Contract);
	UE_LOG(LogHollowGame, Log, TEXT("Raid %s: returning to %s"), *RaidId.ToString(), *Hub);
	GetWorld()->ServerTravel(Hub);
}

FString UHollowRaidFlowComponent::Describe() const
{
	FString Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* Player = It->Get())
		{
			Players += FString::Printf(TEXT(" %s=%s%s"), *Player->GetName(), *HollowEnumToString(GetOutcome(Player)), Player->GetPawn() ? TEXT("") : TEXT("(no pawn)"));
		}
	}
	return FString::Printf(TEXT("raid %s, contract %s, %d loot stack(s), zone %s, players:%s"), *RaidId.ToString(),
		Contract.IsValid() ? *Contract.PrimaryAssetName.ToString() : TEXT("none"), Loot.Num(),
		Zone.IsValid() ? *GetNameSafe(Zone.Get()) : TEXT("not found yet"), *Players);
}
