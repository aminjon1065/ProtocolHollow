#include "Profile/HollowProfileSubsystem.h"

#include "Engine/Engine.h"
#include "Misc/CoreMisc.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HollowEconomyLog.h"
#include "Inventory/HollowLoadoutRules.h"
#include "Items/HollowItemTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Profile/HollowEconomySettings.h"
#include "Profile/HollowProfileSaveGame.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowProfileSubsystem)

UHollowProfileSubsystem* UHollowProfileSubsystem::Get(const UObject* WorldContext)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContext);
	return GameInstance ? GameInstance->GetSubsystem<UHollowProfileSubsystem>() : nullptr;
}

bool UHollowProfileSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !GIsAutomationTesting;
}

void UHollowProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
	// A raid left without a result (crash, alt-F4) counts as abandoned: death rules (ADR-0005).
	FHollowProfileState& State = Save->State;
	HandleReport(HollowProfileRules::ResolveEscrow(State, EHollowRaidOutcome::Abandoned, FDateTime::UtcNow(),
		FHollowItemCatalog::Load(HollowProfileRules::ItemsToCatalog(State))), TEXT("unresolved raid found at startup"));
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
}

void UHollowProfileSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	Super::Deinitialize();
}

const FHollowProfile& UHollowProfileSubsystem::GetProfile() const
{
	return Save->State.Profile;
}

const FHollowRaidEscrow& UHollowProfileSubsystem::GetEscrow() const
{
	return Save->State.Escrow;
}

const FHollowRaidReport* UHollowProfileSubsystem::GetPendingReport() const
{
	return Save->State.bReportPending ? &Save->State.PendingReport : nullptr;
}

TArray<FHollowItemStack> UHollowProfileSubsystem::GetLoadoutForRaid(const FGuid& RaidId)
{
	FHollowProfileState& State = Save->State;
	if (State.Escrow.IsActive() && State.Escrow.RaidId != RaidId)
	{
		HandleReport(HollowProfileRules::ResolveEscrow(State, EHollowRaidOutcome::Abandoned, FDateTime::UtcNow(),
			FHollowItemCatalog::Load(HollowProfileRules::ItemsToCatalog(State))), TEXT("a new raid asks for the loadout before the last one was resolved"));
	}
	// The snapshot carries the slots as they are: the host checks every slot against its stack, so a merged result (two
	// slots of one item as one) would be cut down there. The same rules here only tell what the host will not take.
	TArray<FHollowItemStack> Slots = HollowLoadoutRules::ToStacks(State.Profile.Loadout);
	TArray<FHollowLoadoutIssue> Issues;
	HollowLoadoutRules::ValidateForRaid(Slots, &Issues);
	UE_CLOG(!Issues.IsEmpty(), LogHollowEconomy, Warning, TEXT("Profile: loadout for raid %s not taken as it is: %s"), *RaidId.ToString(),
		*HollowLoadoutRules::Describe(Issues));
	UE_LOG(LogHollowEconomy, Log, TEXT("Profile: loadout for raid %s: %s"), *RaidId.ToString(), *HollowLoadoutRules::Describe(Slots));
	return Slots;
}

void UHollowProfileSubsystem::BeginRaid(const FGuid& RaidId)
{
	FHollowProfileState& State = Save->State;
	if (!RaidId.IsValid() || State.Escrow.RaidId == RaidId)
	{
		return;
	}
	const TOptional<FHollowRaidReport> Previous = HollowProfileRules::BeginRaid(State, RaidId, FDateTime::UtcNow(),
		FHollowItemCatalog::Load(HollowProfileRules::ItemsToCatalog(State)));
	UE_LOG(LogHollowEconomy, Log, TEXT("Profile: raid %s in escrow (%d loadout slots)"), *RaidId.ToString(), State.Escrow.Loadout.Num());
	HandleReport(Previous, TEXT("another raid started before the last one was resolved"));
	Persist();
}

void UHollowProfileSubsystem::ApplyRaidResult(const FHollowRaidResult& Result)
{
	FHollowProfileState& State = Save->State;
	const TOptional<FHollowRaidReport> Report = HollowProfileRules::ApplyResult(State, Result,
		FHollowItemCatalog::Load(HollowProfileRules::ItemsToCatalog(State, &Result)));
	UE_CLOG(!Report, LogHollowEconomy, Warning, TEXT("Profile: result for raid %s ignored (escrow %s)"), *Result.RaidId.ToString(),
		*State.Escrow.RaidId.ToString());
	HandleReport(Report, TEXT("host's result"));
}

EHollowTransactionResult UHollowProfileSubsystem::ApplyTransaction(const FHollowProfileTransaction& Transaction)
{
	FHollowProfileState& State = Save->State;
	const int32 CreditsBefore = State.Profile.Credits;
	const EHollowTransactionResult Result = HollowCraftingRules::Apply(State, Transaction, FHollowEconomyTerms::Load(State, &Transaction));
	UE_LOG(LogHollowEconomy, Log, TEXT("Profile: %s -> %s (credits %d -> %d)"), *Transaction.Describe(), *HollowEnumToString(Result), CreditsBefore,
		State.Profile.Credits);
	if (Result == EHollowTransactionResult::Applied)
	{
		Persist();
		OnProfileChanged.Broadcast();
	}
	return Result;
}

void UHollowProfileSubsystem::AcknowledgeReport()
{
	if (Save->State.bReportPending)
	{
		Save->State.bReportPending = false;
		Persist();
	}
}

bool UHollowProfileSubsystem::SetSlotInsured(const int32 SlotIndex, const bool bInsured)
{
	TArray<FHollowLoadoutSlot>& Loadout = Save->State.Profile.Loadout;
	if (!Loadout.IsValidIndex(SlotIndex))
	{
		return false;
	}
	FHollowLoadoutSlot& Slot = Loadout[SlotIndex];
	if (bInsured && !FHollowItemCatalog::Load(MakeArrayView(&Slot.Item, 1)).IsInsurable(Slot.Item))
	{
		return false;
	}
	Slot.bInsured = bInsured;
	Persist();
	OnProfileChanged.Broadcast();
	return true;
}

bool UHollowProfileSubsystem::SetLoadoutSlot(const FPrimaryAssetId& Item, const int32 Count, const bool bInsured)
{
	const FHollowItemCatalog Catalog = FHollowItemCatalog::Load(MakeArrayView(&Item, 1));
	if (!Catalog.Contains(Item))
	{
		return false;
	}
	TArray<FHollowLoadoutSlot>& Loadout = Save->State.Profile.Loadout;
	FHollowLoadoutSlot* Slot = Loadout.FindByPredicate([&Item](const FHollowLoadoutSlot& S) { return S.Item == Item; });
	if (Count <= 0)
	{
		Loadout.RemoveAll([&Item](const FHollowLoadoutSlot& S) { return S.Item == Item; });
	}
	else
	{
		if (!Slot)
		{
			Slot = &Loadout.AddDefaulted_GetRef();
			Slot->Item = Item;
		}
		Slot->Count = Count;
		Slot->bInsured = bInsured && Catalog.IsInsurable(Item);
	}
	Persist();
	OnProfileChanged.Broadcast();
	return true;
}

void UHollowProfileSubsystem::ResetProfile()
{
	MakeNewProfile();
	Persist();
	OnProfileChanged.Broadcast();
}

void UHollowProfileSubsystem::Load()
{
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	Save = UGameplayStatics::DoesSaveGameExist(Settings->SaveSlotName, Settings->SaveUserIndex)
		? Cast<UHollowProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(Settings->SaveSlotName, Settings->SaveUserIndex)) : nullptr;
	if (Save && Save->Version == UHollowProfileSaveGame::CurrentVersion)
	{
		UE_LOG(LogHollowEconomy, Log, TEXT("Profile loaded from %s: %d credits, %d loadout slots"), *Settings->SaveSlotName,
			Save->State.Profile.Credits, Save->State.Profile.Loadout.Num());
		return;
	}
	UE_CLOG(Save, LogHollowEconomy, Warning, TEXT("Profile %s has version %d (expected %d): starting a new one"), *Settings->SaveSlotName,
		Save->Version, UHollowProfileSaveGame::CurrentVersion);
	Save = Cast<UHollowProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UHollowProfileSaveGame::StaticClass()));
	MakeNewProfile();
	Persist();
}

void UHollowProfileSubsystem::MakeNewProfile()
{
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	Save->State = FHollowProfileState();
	Save->State.Profile.Credits = Settings->StartingCredits;
	Save->State.Profile.Loadout = Settings->StartingLoadout;
	Save->State.Profile.Stash = Settings->StartingStash;
	UE_LOG(LogHollowEconomy, Log, TEXT("Profile: new profile with %d credits, %d loadout slots and %d stash stacks"), Settings->StartingCredits,
		Settings->StartingLoadout.Num(), Settings->StartingStash.Num());
}

void UHollowProfileSubsystem::Persist() const
{
	const UHollowEconomySettings* Settings = GetDefault<UHollowEconomySettings>();
	UE_CLOG(!UGameplayStatics::SaveGameToSlot(Save, Settings->SaveSlotName, Settings->SaveUserIndex), LogHollowEconomy, Error,
		TEXT("Profile could not be saved to %s"), *Settings->SaveSlotName);
}

void UHollowProfileSubsystem::HandleReport(const TOptional<FHollowRaidReport>& Report, const TCHAR* Why)
{
	if (!Report)
	{
		return;
	}
	UE_LOG(LogHollowEconomy, Log, TEXT("Profile: raid %s resolved as %s (%s): credits %d -> %d, %d item(s) kept, %d lost, %d sold, %d insurance(s) used"),
		*Report->Result.RaidId.ToString(), *HollowEnumToString(Report->Result.Outcome), Why, Report->CreditsBefore, Report->CreditsAfter,
		Report->Delta.Kept.Num(), Report->Delta.Lost.Num(), Report->Delta.Sold.Num(), Report->Delta.InsuranceUsed.Num());
	Persist();
	OnRaidReport.Broadcast(Report.GetValue());
	OnProfileChanged.Broadcast();
}

void UHollowProfileSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// Only a client loses its host; a host's own failures never end the raid it runs.
	if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() != NM_Client || !Save->State.Escrow.IsActive())
	{
		return;
	}
	FHollowProfileState& State = Save->State;
	HandleReport(HollowProfileRules::ResolveEscrow(State, EHollowRaidOutcome::SignalLost, FDateTime::UtcNow(),
		FHollowItemCatalog::Load(HollowProfileRules::ItemsToCatalog(State))), *FString::Printf(TEXT("connection to the host lost: %s"),
		ENetworkFailure::ToString(FailureType)));
}
