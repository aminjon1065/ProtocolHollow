#include "ViewModels/HollowObjectiveModel.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarryableComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Crafting/HollowCraftingRules.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Extraction/HollowExtractionResolver.h"
#include "GameFramework/PlayerController.h"
#include "HollowUILog.h"
#include "Hub/HollowHubScreenRules.h"
#include "Hub/HollowHubTerminal.h"
#include "Hub/HollowHubTerminalSubsystem.h"
#include "Items/HollowItemTypes.h"
#include "Profile/HollowProfileSource.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Profile/HollowProfileTypes.h"
#include "Raid/HollowExtractionZone.h"
#include "Raid/HollowRaidLauncher.h"
#include "Raid/HollowRaidStateComponent.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "ViewModels/HollowVisorContext.h"
#include "World/HollowRitualAltar.h"
#include "World/HollowRitualAltarDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowObjectiveModel)

namespace HollowObjectiveModelPrivate
{
	/** The shuttle's countdown on the tracker redraws this often. */
	constexpr float CountdownSeconds = 1.f;

	template <typename TActor>
	TActor* Nearest(const TArray<TWeakObjectPtr<TActor>>& Actors, const FVector& From, TFunctionRef<bool(const TActor&)> Accept)
	{
		TActor* Best = nullptr;
		double BestDistance = TNumericLimits<double>::Max();
		for (const TWeakObjectPtr<TActor>& Weak : Actors)
		{
			TActor* Actor = Weak.Get();
			const double Distance = Actor ? FVector::DistSquared(From, Actor->GetActorLocation()) : 0.0;
			if (Actor && Accept(*Actor) && Distance < BestDistance)
			{
				Best = Actor;
				BestDistance = Distance;
			}
		}
		return Best;
	}

	EHollowWaypointKind KindOf(const EHollowTerminalKind Kind)
	{
		switch (Kind)
		{
		case EHollowTerminalKind::Contracts:
			return EHollowWaypointKind::Contracts;
		case EHollowTerminalKind::Workbench:
			return EHollowWaypointKind::Workbench;
		case EHollowTerminalKind::Locker:
			break;
		}
		return EHollowWaypointKind::Locker;
	}
}

void UHollowObjectiveModel::Initialize(UHollowVisorContext& InContext)
{
	Context = &InContext;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::BindSources);
	SpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));
	if (UHollowHubTerminalSubsystem* Terminal = UHollowHubTerminalSubsystem::Get(World))
	{
		OpenedHandle = Terminal->OnTerminalOpened.AddUObject(this, &ThisClass::HandleTerminalOpened);
		ClosedHandle = Terminal->OnTerminalClosed.AddUObject(this, &ThisClass::HandleTerminalClosed);
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		Track(*It);
	}
	BindSources();
}

void UHollowObjectiveModel::Deinitialize()
{
	UWorld* World = GetWorld();
	if (UHollowVisorContext* Owner = Context.Get())
	{
		Owner->OnSourcesChanged.Remove(SourcesHandle);
	}
	if (World)
	{
		World->RemoveOnActorSpawnedHandler(SpawnedHandle);
		World->GetTimerManager().ClearTimer(CountdownTimer);
		if (UHollowHubTerminalSubsystem* Terminal = UHollowHubTerminalSubsystem::Get(World))
		{
			Terminal->OnTerminalOpened.Remove(OpenedHandle);
			Terminal->OnTerminalClosed.Remove(ClosedHandle);
		}
	}
	if (UHollowRaidStateComponent* Raid = RaidState.Get())
	{
		Raid->OnStateChanged.Remove(RaidHandle);
	}
	if (UHollowAbilitySystemComponent* Abilities = AbilitySystem.Get())
	{
		Abilities->RegisterGameplayTagEvent(HollowTags::State_Dead, EGameplayTagEventType::NewOrRemoved).Remove(DeadHandle);
	}
	for (const FTrackedCore& Tracked : Cores)
	{
		if (const AHollowAetherCore* Core = Tracked.Core.Get())
		{
			Core->GetCarryable()->OnCarryStateChanged.RemoveAll(this);
			Core->GetCarryable()->OnLootItemChanged.RemoveAll(this);
		}
	}
	for (const TWeakObjectPtr<AHollowRitualAltar>& Weak : Altars)
	{
		if (AHollowRitualAltar* Altar = Weak.Get())
		{
			Altar->OnAltarStateChanged.RemoveAll(this);
		}
	}
	Cores.Reset();
	Altars.Reset();
	Zones.Reset();
	Terminals.Reset();
	RaidState.Reset();
	AbilitySystem.Reset();
	Context.Reset();
}

UWorld* UHollowObjectiveModel::GetWorld() const
{
	const UHollowVisorContext* Owner = Context.Get();
	return Owner ? Owner->GetWorld() : nullptr;
}

void UHollowObjectiveModel::SetProfileSourceForTests(IHollowProfileSource* Source)
{
	TestSource = Source;
	bPriced = false;
	for (const TWeakObjectPtr<AHollowRitualAltar>& Weak : Altars)
	{
		if (const AHollowRitualAltar* Altar = Weak.Get())
		{
			UpdatePrices(*Altar);
		}
	}
	Refresh();
}

void UHollowObjectiveModel::BindSources()
{
	UWorld* World = GetWorld();
	UHollowRaidStateComponent* NewRaid = UHollowRaidStateComponent::Get(World);
	if (RaidState.Get() != NewRaid)
	{
		if (UHollowRaidStateComponent* Old = RaidState.Get())
		{
			Old->OnStateChanged.Remove(RaidHandle);
		}
		RaidState = NewRaid;
		if (NewRaid)
		{
			RaidHandle = NewRaid->OnStateChanged.AddUObject(this, &ThisClass::HandleRaidState);
		}
	}
	UHollowAbilitySystemComponent* NewAbilities = Context.IsValid() ? Context->GetAbilitySystem() : nullptr;
	if (AbilitySystem.Get() != NewAbilities)
	{
		if (UHollowAbilitySystemComponent* Old = AbilitySystem.Get())
		{
			Old->RegisterGameplayTagEvent(HollowTags::State_Dead, EGameplayTagEventType::NewOrRemoved).Remove(DeadHandle);
		}
		AbilitySystem = NewAbilities;
		if (NewAbilities)
		{
			DeadHandle = NewAbilities->RegisterGameplayTagEvent(HollowTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleDeadTag);
		}
	}
	Refresh();
}

void UHollowObjectiveModel::HandleActorSpawned(AActor* Actor)
{
	if (Track(Actor))
	{
		Refresh();   // only a place or a core the objective knows changes it (debris and sounds spawn all the time)
	}
}

bool UHollowObjectiveModel::Track(AActor* Actor)
{
	if (AHollowAetherCore* Core = Cast<AHollowAetherCore>(Actor))
	{
		if (Cores.ContainsByPredicate([Core](const FTrackedCore& Tracked) { return Tracked.Core == Core; }))
		{
			return false;
		}
		Cores.Add({Core, Core->GetCarryable()->GetCarryState().IsCarried()});
		Core->GetCarryable()->OnCarryStateChanged.AddUObject(this, &ThisClass::HandleCarry);
		Core->GetCarryable()->OnLootItemChanged.AddUObject(this, &ThisClass::Refresh);
		return true;
	}
	if (AHollowRitualAltar* Altar = Cast<AHollowRitualAltar>(Actor))
	{
		if (Altars.Contains(Altar))
		{
			return false;
		}
		Altars.Add(Altar);
		Altar->OnAltarStateChanged.AddWeakLambda(this, [this](const AHollowRitualAltar&) { Refresh(); });
		UpdatePrices(*Altar);
		return true;
	}
	if (AHollowExtractionZone* Zone = Cast<AHollowExtractionZone>(Actor))
	{
		return Zones.AddUnique(Zone) == Zones.Num() - 1;
	}
	if (AHollowHubTerminal* Terminal = Cast<AHollowHubTerminal>(Actor))
	{
		return Terminals.AddUnique(Terminal) == Terminals.Num() - 1;
	}
	return false;
}

void UHollowObjectiveModel::HandleRaidState(const FHollowRaidNetState& OldState, const FHollowRaidNetState& NewState)
{
	Refresh();
}

void UHollowObjectiveModel::HandleCarry(const FHollowCarryNetState& OldState, const FHollowCarryNetState& NewState)
{
	if (NewState.IsCarried())
	{
		for (FTrackedCore& Tracked : Cores)
		{
			const AHollowAetherCore* Core = Tracked.Core.Get();
			Tracked.bLifted |= Core && Core->GetCarryable()->GetCarryState().IsCarried();
		}
	}
	Refresh();
}

void UHollowObjectiveModel::HandleTerminalOpened(APlayerController* Player, AHollowHubTerminal* Terminal)
{
	if (!Context.IsValid() || Player != Context->GetPlayer())
	{
		return;
	}
	bScreenOpen = true;
	bPrepared |= Terminal && Terminal->GetKind() != EHollowTerminalKind::Contracts;
	Refresh();
}

void UHollowObjectiveModel::HandleTerminalClosed(APlayerController* Player, AHollowHubTerminal* Terminal)
{
	if (Context.IsValid() && Player == Context->GetPlayer())
	{
		bScreenOpen = false;
		Refresh();
	}
}

void UHollowObjectiveModel::HandleDeadTag(const FGameplayTag Tag, const int32 Count)
{
	Refresh();
}

void UHollowObjectiveModel::UpdatePrices(const AHollowRitualAltar& Altar)
{
	const UHollowRitualAltarDefinition* Definition = Altar.GetDefinition();
	if (bPriced || !Definition)
	{
		return;
	}
	// Priced for this player: their own ship's core bonus, by the extraction's own rule (as the contracts screen, ADR-0041).
	const IHollowProfileSource* Source = TestSource ? TestSource : UHollowProfileSubsystem::Get(this);
	int32 BonusPercent = 0;
	if (Source)
	{
		const FHollowProfileState& State = Source->GetProfileState();
		BonusPercent = HollowHubScreenRules::CoreValuePercent(State, FHollowEconomyTerms::Load(State));
	}
	const FHollowItemCatalog Catalog = FHollowItemCatalog::Load({Definition->AcceptedItem, Definition->ResultItem});
	Facts.CoreCredits = FHollowExtractionResolver::ItemEarnings(Catalog, Definition->AcceptedItem, 1, BonusPercent);
	Facts.PurifiedCredits = FHollowExtractionResolver::ItemEarnings(Catalog, Definition->ResultItem, 1, BonusPercent);
	bPriced = true;
}

void UHollowObjectiveModel::GatherFacts(FHollowObjectiveFacts& OutFacts)
{
	const UWorld* World = GetWorld();
	const UHollowRaidStateComponent* Raid = RaidState.Get();
	const FHollowRaidNetState* Net = Raid ? &Raid->GetState() : nullptr;
	OutFacts.bInRaid = Net && Net->RaidId.IsValid();
	OutFacts.bInHub = !OutFacts.bInRaid && (Terminals.Num() > 0 || HollowRaidLauncher::IsInHub(World));
	OutFacts.bHost = World && World->GetNetMode() != NM_Client;
	OutFacts.bPrepared = bPrepared;
	OutFacts.Phase = Net ? Net->Phase : EHollowRaidPhase::Assembling;
	OutFacts.SecondsLeft = Raid ? FMath::CeilToInt32(Raid->GetSecondsLeft()) : 0;
	if (Net && Net->RaidId != RaidId)
	{
		RaidId = Net->RaidId;   // a new raid: nothing has left its vault yet (cores are this raid's anyway)
		for (FTrackedCore& Tracked : Cores)
		{
			const AHollowAetherCore* Core = Tracked.Core.Get();
			Tracked.bLifted = Core && Core->GetCarryable()->GetCarryState().IsCarried();
		}
	}
	const AHollowExoFrameCharacter* Body = Context.IsValid() ? Context->GetCharacter() : nullptr;
	const UHollowAbilitySystemComponent* Abilities = AbilitySystem.Get();
	OutFacts.bAlive = Body && !Body->IsDead() && !(Abilities && Abilities->HasMatchingGameplayTag(HollowTags::State_Dead));

	OutFacts.bAltarIdle = false;
	OutFacts.bRitual = false;
	TArray<FPrimaryAssetId, TInlineAllocator<2>> Accepted;
	for (const TWeakObjectPtr<AHollowRitualAltar>& Weak : Altars)
	{
		const AHollowRitualAltar* Altar = Weak.Get();
		if (!Altar || !Altar->GetDefinition())
		{
			continue;
		}
		const EHollowAltarPhase Phase = Altar->GetAltarState().Phase;
		OutFacts.bAltarIdle |= Phase == EHollowAltarPhase::Idle;
		OutFacts.bRitual |= Phase == EHollowAltarPhase::Ritual;
		if (Phase == EHollowAltarPhase::Idle)
		{
			Accepted.AddUnique(Altar->GetDefinition()->AcceptedItem);
		}
	}
	OutFacts.bCoreLifted = false;
	OutFacts.bPurifiableCore = false;
	for (const FTrackedCore& Tracked : Cores)
	{
		const AHollowAetherCore* Core = Tracked.Core.Get();
		if (!Core || !Tracked.bLifted)
		{
			continue;
		}
		const UHollowCarryableComponent* Carryable = Core->GetCarryable();
		OutFacts.bCoreLifted = true;
		OutFacts.bPurifiableCore |= !Carryable->IsHeld() && Accepted.Contains(Carryable->GetLootItem());
	}
}

void UHollowObjectiveModel::Refresh()
{
	if (!Context.IsValid())
	{
		return;
	}
	GatherFacts(Facts);
	const FHollowObjective Next = HollowObjectiveRules::Pick(Facts);
	UpdateCountdown();
	if (Next.SameAs(Objective) && bScreenOpen == bAnnouncedScreenOpen)
	{
		return;
	}
	const EHollowObjectiveStep OldStep = Objective.Step;
	Objective = Next;
	bAnnouncedScreenOpen = bScreenOpen;   // the waypoints step aside under a screen even when the step stays
	UE_CLOG(OldStep != Objective.Step, LogHollowUI, Log, TEXT("Objective of %s: %s"), *GetNameSafe(Context->GetPlayer()), *Describe());
	OnObjectiveChanged.Broadcast();
}

void UHollowObjectiveModel::UpdateCountdown()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerManager& Timers = World->GetTimerManager();
	const bool bTimed = HollowObjectiveRules::IsTimed(HollowObjectiveRules::PickStep(Facts));
	if (bTimed && !Timers.IsTimerActive(CountdownTimer))
	{
		Timers.SetTimer(CountdownTimer, this, &ThisClass::Refresh, HollowObjectiveModelPrivate::CountdownSeconds, /*bLoop=*/true);
	}
	else if (!bTimed)
	{
		Timers.ClearTimer(CountdownTimer);
	}
}

void UHollowObjectiveModel::GetWaypoints(const FVector& From, TArray<FHollowWaypointTarget>& OutTargets) const
{
	using namespace HollowObjectiveModelPrivate;
	OutTargets.Reset();
	for (const EHollowWaypointKind Kind : Objective.Waypoints)
	{
		TOptional<FVector> Location;
		switch (Kind)
		{
		case EHollowWaypointKind::Contracts:
		case EHollowWaypointKind::Workbench:
		case EHollowWaypointKind::Locker:
			if (const AHollowHubTerminal* Terminal = Nearest<AHollowHubTerminal>(Terminals, From, [Kind](const AHollowHubTerminal& Candidate) { return KindOf(Candidate.GetKind()) == Kind; }))
			{
				Location = Terminal->GetActorLocation();
			}
			break;
		case EHollowWaypointKind::Extraction:
			if (const AHollowExtractionZone* Zone = Nearest<AHollowExtractionZone>(Zones, From, [](const AHollowExtractionZone&) { return true; }))
			{
				Location = Zone->GetActorLocation();
			}
			break;
		case EHollowWaypointKind::Altar:
			if (const AHollowRitualAltar* Altar = Nearest<AHollowRitualAltar>(Altars, From, [](const AHollowRitualAltar& Candidate) { return Candidate.GetAltarState().Phase != EHollowAltarPhase::Spent; }))
			{
				Location = Altar->GetSeatTransform().GetLocation();
			}
			break;
		case EHollowWaypointKind::Vault:
		{
			// The cores still resting where the sector put them: the vault itself.
			double Best = TNumericLimits<double>::Max();
			for (const FTrackedCore& Tracked : Cores)
			{
				const AHollowAetherCore* Core = Tracked.Core.Get();
				const double Distance = Core ? FVector::DistSquared(From, Core->GetActorLocation()) : 0.0;
				if (Core && !Tracked.bLifted && Distance < Best)
				{
					Best = Distance;
					Location = Core->GetActorLocation();
				}
			}
			break;
		}
		}
		if (Location.IsSet())
		{
			OutTargets.Add({Kind, Location.GetValue()});
		}
	}
}

FString UHollowObjectiveModel::Describe() const
{
	TArray<FString> Kinds;
	for (const EHollowWaypointKind Kind : Objective.Waypoints)
	{
		Kinds.Add(StaticEnum<EHollowWaypointKind>()->GetNameStringByValue(static_cast<int64>(Kind)));
	}
	return FString::Printf(TEXT("objective %s \"%s\" waypoints [%s]%s (lifted %d, purifiable %d, altar idle %d, ritual %d, alive %d)"),
		*StaticEnum<EHollowObjectiveStep>()->GetNameStringByValue(static_cast<int64>(Objective.Step)), *Objective.Text.ToString(),
		*FString::Join(Kinds, TEXT(",")), bScreenOpen ? TEXT(" under a screen") : TEXT(""), Facts.bCoreLifted, Facts.bPurifiableCore,
		Facts.bAltarIdle, Facts.bRitual, Facts.bAlive);
}
