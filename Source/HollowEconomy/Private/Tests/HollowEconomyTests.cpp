#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Extraction/HollowExtractionResolver.h"
#include "Kismet/GameplayStatics.h"
#include "Profile/HollowProfileSaveGame.h"
#include "Profile/HollowProfileTypes.h"

namespace HollowEconomyTests
{
	// Test data only: real items are DA_Item_* (create_raid_flow_assets.py).
	const FPrimaryAssetId Welder(TEXT("HollowItem"), TEXT("Test_Welder"));
	const FPrimaryAssetId Cutter(TEXT("HollowItem"), TEXT("Test_Cutter"));
	const FPrimaryAssetId FuelCell(TEXT("HollowItem"), TEXT("Test_FuelCell"));
	const FPrimaryAssetId Core(TEXT("HollowItem"), TEXT("Test_Core"));
	constexpr int32 WelderValue = 300;
	constexpr int32 CutterValue = 400;
	constexpr int32 FuelCellValue = 25;
	constexpr int32 CoreValue = 500;
	constexpr int32 FuelCells = 3;
	constexpr int32 Cores = 2;
	constexpr int32 StartingCredits = 1000;
	const TCHAR* const TestSlot = TEXT("HollowProfileAutomationTest");

	FHollowItemCatalog MakeCatalog()
	{
		FHollowItemCatalog Catalog;
		Catalog.Add(Welder, WelderValue, /*bInsurable=*/true);
		Catalog.Add(Cutter, CutterValue, /*bInsurable=*/true);
		Catalog.Add(FuelCell, FuelCellValue, /*bInsurable=*/false);
		Catalog.Add(Core, CoreValue, /*bInsurable=*/false);
		return Catalog;
	}

	FHollowLoadoutSlot Slot(const FPrimaryAssetId& Item, const int32 Count, const bool bInsured)
	{
		FHollowLoadoutSlot Result;
		Result.Item = Item;
		Result.Count = Count;
		Result.bInsured = bInsured;
		return Result;
	}

	FHollowItemStack Stack(const FPrimaryAssetId& Item, const int32 Count)
	{
		FHollowItemStack Result;
		Result.Item = Item;
		Result.Count = Count;
		return Result;
	}

	/** Welder insured, cutter not, fuel cells marked insured although fuel cannot be insured. */
	FHollowRaidContext MakeContext()
	{
		FHollowRaidContext Context;
		Context.Loadout = {Slot(Welder, 1, true), Slot(Cutter, 1, false), Slot(FuelCell, FuelCells, true)};
		Context.Loot = {Stack(Core, Cores)};
		return Context;
	}

	FHollowProfile MakeProfile()
	{
		FHollowProfile Profile;
		Profile.Credits = StartingCredits;
		Profile.Loadout = MakeContext().Loadout;
		return Profile;
	}

	int32 CountOf(const TArray<FHollowItemStack>& Stacks, const FPrimaryAssetId& Item)
	{
		const FHollowItemStack* Found = Stacks.FindByPredicate([&Item](const FHollowItemStack& S) { return S.Item == Item; });
		return Found ? Found->Count : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResolverExtractionTest, "Hollow.Economy.Resolver.ExtractionKeepsEverything",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResolverExtractionTest::RunTest(const FString& Parameters)
{
	using namespace HollowEconomyTests;
	const FHollowProfileDelta Delta = FHollowExtractionResolver::Resolve(MakeContext(), EHollowRaidOutcome::Extracted, MakeCatalog());
	TestEqual(TEXT("The whole loadout is kept"), Delta.Kept.Num(), 3);
	TestTrue(TEXT("Uninsured and uninsurable items too"), CountOf(Delta.Kept, Cutter) == 1 && CountOf(Delta.Kept, FuelCell) == FuelCells);
	TestTrue(TEXT("Nothing is lost"), Delta.Lost.IsEmpty());
	TestEqual(TEXT("The cores are sold"), CountOf(Delta.Sold, Core), Cores);
	TestEqual(TEXT("For their value"), Delta.Credits, Cores * CoreValue);

	FHollowProfile Profile = MakeProfile();
	HollowProfileRules::Apply(Profile, Delta, EHollowRaidOutcome::Extracted);
	TestEqual(TEXT("Credits grow by the sale"), Profile.Credits, StartingCredits + Cores * CoreValue);
	TestTrue(TEXT("The loadout is unchanged, insurance included (it saved nothing)"), Profile.Loadout == MakeContext().Loadout && Delta.InsuranceUsed.IsEmpty());
	TestTrue(TEXT("Counters"), Profile.Stats.Raids == 1 && Profile.Stats.Extractions == 1 && Profile.Stats.Deaths == 0 && Profile.Stats.CoresDelivered == Cores);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResolverDeathTest, "Hollow.Economy.Resolver.DeathKeepsOnlyInsured",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResolverDeathTest::RunTest(const FString& Parameters)
{
	using namespace HollowEconomyTests;
	const FHollowItemCatalog Catalog = MakeCatalog();
	for (const EHollowRaidOutcome Outcome : {EHollowRaidOutcome::Dead, EHollowRaidOutcome::MissingInAction, EHollowRaidOutcome::Abandoned})
	{
		const FString What = UEnum::GetValueAsString(Outcome);
		const FHollowProfileDelta Delta = FHollowExtractionResolver::Resolve(MakeContext(), Outcome, Catalog);
		TestTrue(What + TEXT(": only the insured welder is kept"), Delta.Kept.Num() == 1 && CountOf(Delta.Kept, Welder) == 1);
		TestEqual(What + TEXT(": the uninsured cutter is lost"), CountOf(Delta.Lost, Cutter), 1);
		TestEqual(What + TEXT(": fuel cannot be insured, even when marked"), CountOf(Delta.Lost, FuelCell), FuelCells);
		TestTrue(What + TEXT(": the loot is lost, nothing earned"), Delta.Sold.IsEmpty() && Delta.Credits == 0);
		TestTrue(What + TEXT(": the welder's insurance paid out"), Delta.InsuranceUsed.Num() == 1 && CountOf(Delta.InsuranceUsed, Welder) == 1);

		FHollowProfile Profile = MakeProfile();
		HollowProfileRules::Apply(Profile, Delta, Outcome);
		TestEqual(What + TEXT(": credits already earned stay"), Profile.Credits, StartingCredits);
		TestTrue(What + TEXT(": the loadout keeps the welder only, its insurance spent (single-use, ADR-0031)"), Profile.Loadout.Num() == 1
			&& Profile.Loadout[0] == Slot(Welder, 1, false));
		TestTrue(What + TEXT(": counted as a death"), Profile.Stats.Deaths == 1 && Profile.Stats.Extractions == 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResolverSignalLostTest, "Hollow.Economy.Resolver.SignalLost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResolverSignalLostTest::RunTest(const FString& Parameters)
{
	using namespace HollowEconomyTests;
	const FHollowProfileDelta Delta = FHollowExtractionResolver::Resolve(MakeContext(), EHollowRaidOutcome::SignalLost, MakeCatalog());
	TestEqual(TEXT("The host vanished: the loadout is kept"), Delta.Kept.Num(), 3);
	TestTrue(TEXT("Nothing lost"), Delta.Lost.IsEmpty());
	TestTrue(TEXT("The loot is lost"), Delta.Sold.IsEmpty() && Delta.Credits == 0);
	TestTrue(TEXT("No outcome yet changes nothing"), FHollowExtractionResolver::Resolve(MakeContext(), EHollowRaidOutcome::None, MakeCatalog()).Kept.IsEmpty());

	FHollowItemCatalog Empty;
	const FHollowProfileDelta Unknown = FHollowExtractionResolver::Resolve(MakeContext(), EHollowRaidOutcome::Dead, Empty);
	TestTrue(TEXT("An item unknown to the catalog is never insured"), Unknown.Kept.IsEmpty() && CountOf(Unknown.Lost, Welder) == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowProfileEscrowTest, "Hollow.Economy.Profile.Escrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowProfileEscrowTest::RunTest(const FString& Parameters)
{
	using namespace HollowEconomyTests;
	const FHollowItemCatalog Catalog = MakeCatalog();
	const FDateTime Now(2026, 11, 20, 12, 0, 0);
	const FGuid RaidA = FGuid::NewGuid();
	const FGuid RaidB = FGuid::NewGuid();
	FHollowProfileState State;
	State.Profile = MakeProfile();

	// Into the raid: a snapshot of the loadout, nothing resolved.
	TestFalse(TEXT("A first raid resolves nothing"), HollowProfileRules::BeginRaid(State, RaidA, Now, Catalog).IsSet());
	TestTrue(TEXT("Escrow with the loadout snapshot"), State.Escrow.RaidId == RaidA && State.Escrow.Loadout == State.Profile.Loadout);
	TestFalse(TEXT("The same raid again changes nothing"), HollowProfileRules::BeginRaid(State, RaidA, Now, Catalog).IsSet());

	// A result for another raid is ignored; the right one resolves and clears the escrow.
	FHollowRaidResult Result;
	Result.RaidId = RaidB;
	Result.Outcome = EHollowRaidOutcome::Extracted;
	Result.Loot = {Stack(Core, 1)};
	TestFalse(TEXT("Another raid's result is ignored"), HollowProfileRules::ApplyResult(State, Result, Catalog).IsSet());
	Result.RaidId = RaidA;
	const TOptional<FHollowRaidReport> Report = HollowProfileRules::ApplyResult(State, Result, Catalog);
	TestTrue(TEXT("Resolved by the host"), Report.IsSet() && Report->CreditsBefore == StartingCredits && Report->CreditsAfter == StartingCredits + CoreValue);
	TestTrue(TEXT("Escrow cleared, debrief pending"), !State.Escrow.IsActive() && State.bReportPending);
	TestFalse(TEXT("A second copy of the result changes nothing"), HollowProfileRules::ApplyResult(State, Result, Catalog).IsSet());

	// A raid left without a result: the next raid resolves it as abandoned first (death rules).
	HollowProfileRules::BeginRaid(State, RaidA, Now, Catalog);
	const TOptional<FHollowRaidReport> Abandoned = HollowProfileRules::BeginRaid(State, RaidB, Now + FTimespan::FromMinutes(5.0), Catalog);
	TestTrue(TEXT("The unresolved raid counts as abandoned"), Abandoned.IsSet() && Abandoned->Result.Outcome == EHollowRaidOutcome::Abandoned);
	TestTrue(TEXT("Its time is the time spent"), Abandoned.IsSet() && FMath::IsNearlyEqual(Abandoned->Result.RaidSeconds, 300.f));
	TestTrue(TEXT("Only the insured welder survives"), State.Profile.Loadout.Num() == 1 && State.Profile.Loadout[0].Item == Welder);
	TestTrue(TEXT("The new raid is in escrow"), State.Escrow.RaidId == RaidB);

	// The host vanishes: the loadout is kept.
	const TOptional<FHollowRaidReport> Lost = HollowProfileRules::ResolveEscrow(State, EHollowRaidOutcome::SignalLost, Now, Catalog);
	TestTrue(TEXT("Signal lost keeps the loadout"), Lost.IsSet() && Lost->Delta.Lost.IsEmpty() && State.Profile.Loadout.Num() == 1);
	TestFalse(TEXT("Nothing left to resolve"), HollowProfileRules::ResolveEscrow(State, EHollowRaidOutcome::Abandoned, Now, Catalog).IsSet());

	// On disk: an escrow written before a crash is found again by the next launch, which resolves it as abandoned.
	State.Profile = MakeProfile();
	HollowProfileRules::BeginRaid(State, RaidA, Now, Catalog);
	UHollowProfileSaveGame* Save = Cast<UHollowProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UHollowProfileSaveGame::StaticClass()));
	Save->State = State;
	if (!TestTrue(TEXT("Saved"), UGameplayStatics::SaveGameToSlot(Save, TestSlot, 0)))
	{
		return false;
	}
	UHollowProfileSaveGame* Loaded = Cast<UHollowProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(TestSlot, 0));
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
	if (!TestNotNull(TEXT("Loaded"), Loaded))
	{
		return false;
	}
	TestEqual(TEXT("Same version"), Loaded->Version, UHollowProfileSaveGame::CurrentVersion);
	TestTrue(TEXT("The escrow survived on disk"), Loaded->State.Escrow.RaidId == RaidA && Loaded->State.Escrow.Loadout == MakeProfile().Loadout);
	const TOptional<FHollowRaidReport> Crash = HollowProfileRules::ResolveEscrow(Loaded->State, EHollowRaidOutcome::Abandoned, Now, Catalog);
	TestTrue(TEXT("Next launch: abandoned, death rules"), Crash.IsSet() && CountOf(Crash->Delta.Lost, Cutter) == 1 && Loaded->State.Profile.Loadout.Num() == 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
