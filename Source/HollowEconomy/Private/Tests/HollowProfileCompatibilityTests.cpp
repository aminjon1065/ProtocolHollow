#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/Base64.h"
#include "Profile/HollowProfileSaveGame.h"
#include "Profile/HollowProfileTypes.h"
#include "Tests/HollowProfileFixtures.h"

namespace HollowProfileCompatibilityTests
{
	const FPrimaryAssetId FuelCell(TEXT("HollowItem"), TEXT("DA_Item_FuelCell"));
	const FPrimaryAssetId PulseMine(TEXT("HollowItem"), TEXT("DA_Item_PulseMine"));
	const FPrimaryAssetId Nails(TEXT("HollowItem"), TEXT("DA_Item_AmmoNails"));

	UHollowProfileSaveGame* Load(const TArray<uint8>& Bytes)
	{
		return Cast<UHollowProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowProfileCompatibilityTest, "Hollow.Economy.Profile.Compatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowProfileCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace HollowProfileCompatibilityTests;
	// A save written by the code before NAR-01 (HollowProfileFixtures.h): the profile's new fields are tagged
	// properties, so the old file loads as it was, with nothing in them, and stays the same version.
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("The fixture decodes"), FBase64::Decode(FString(HollowProfileFixtures::PreNar01Save()), Bytes)))
	{
		return false;
	}
	const UHollowProfileSaveGame* Old = Load(Bytes);
	if (!TestNotNull(TEXT("A profile from before NAR-01 loads"), Old))
	{
		return false;
	}
	TestEqual(TEXT("Same save version (no incompatible change)"), Old->Version, UHollowProfileSaveGame::CurrentVersion);
	const FHollowProfile& Profile = Old->State.Profile;
	TestEqual(TEXT("Credits kept"), Profile.Credits, 1234);
	const TArray<FHollowLoadoutSlot> Loadout = {{FuelCell, 3, true}, {PulseMine, 2, false}};
	TestTrue(TEXT("Loadout kept"), Profile.Loadout == Loadout);
	TestTrue(TEXT("Stash kept"), Profile.Stash.Num() == 2 && Profile.Stash[0].Item == FuelCell && Profile.Stash[0].Count == 5
		&& Profile.Stash[1].Item == Nails && Profile.Stash[1].Count == 40);
	TestTrue(TEXT("Upgrades kept"), Profile.Upgrades == TArray<FPrimaryAssetId>{FPrimaryAssetId(TEXT("HollowUpgrade"), TEXT("DA_Upgrade_FuelReserve"))});
	TestEqual(TEXT("Class kept"), Profile.ClassId, FPrimaryAssetId(TEXT("HollowClass"), TEXT("DA_Class_Occultist")));
	TestTrue(TEXT("The other class's loadout kept"), Profile.ClassLoadouts.Num() == 1
		&& Profile.ClassLoadouts[0].ClassId == FPrimaryAssetId(TEXT("HollowClass"), TEXT("DA_Class_Engineer")) && Profile.ClassLoadouts[0].Loadout.Num() == 1);
	TestTrue(TEXT("Career kept"), Profile.Stats.Raids == 7 && Profile.Stats.Extractions == 4 && Profile.Stats.Deaths == 3 && Profile.Stats.CoresDelivered == 5);
	TestTrue(TEXT("Recent transactions kept"), Old->State.RecentTransactions == TArray<FGuid>{FGuid(1, 2, 3, 4)});
	TestFalse(TEXT("No raid in escrow"), Old->State.Escrow.IsActive());
	TestTrue(TEXT("NAR-01a: no radio cue heard yet"), Profile.RadioHeard.IsEmpty());
	TestTrue(TEXT("NAR-01b: no crew log found yet, none waiting for a bonus"), Profile.Lore.IsEmpty() && Old->State.Escrow.NewLore.IsEmpty());

	// What NAR-01 adds survives a save and a load.
	UHollowProfileSaveGame* Save = NewObject<UHollowProfileSaveGame>();
	Save->State = Old->State;
	Save->State.Profile.RadioHeard = {TEXT("Hub.Briefing")};
	Save->State.Profile.Lore = {FPrimaryAssetId(TEXT("HollowLore"), TEXT("DA_LoreEntry_Docks_01"))};
	TArray<uint8> Saved;
	const UHollowProfileSaveGame* Again = TestTrue(TEXT("Saved"), UGameplayStatics::SaveGameToMemory(Save, Saved)) ? Load(Saved) : nullptr;
	TestTrue(TEXT("Radio cues heard are kept"), Again && Again->State.Profile.RadioHeard == TArray<FName>{TEXT("Hub.Briefing")});
	TestTrue(TEXT("Crew logs found are kept"), Again && Again->State.Profile.Lore == Save->State.Profile.Lore);
	TestTrue(TEXT("And the rest with them"), Again && Again->State.Profile.Credits == 1234 && Again->State.Profile.Loadout == Loadout);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowProfileRadioHeardTest, "Hollow.Economy.Profile.RadioHeard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowProfileRadioHeardTest::RunTest(const FString& Parameters)
{
	FHollowProfile Profile;
	TestTrue(TEXT("A first cue is remembered"), HollowProfileRules::MarkRadioHeard(Profile, TEXT("Hub.Briefing")));
	TestFalse(TEXT("The same cue again changes nothing"), HollowProfileRules::MarkRadioHeard(Profile, TEXT("Hub.Briefing")));
	TestFalse(TEXT("A cue without an id is never remembered"), HollowProfileRules::MarkRadioHeard(Profile, NAME_None));
	TestTrue(TEXT("Another cue is"), HollowProfileRules::MarkRadioHeard(Profile, TEXT("Sortie.Docks")));
	TestTrue(TEXT("In the order heard"), Profile.RadioHeard == (TArray<FName>{TEXT("Hub.Briefing"), TEXT("Sortie.Docks")}));

	// A death keeps what was said: the heard cues are no item of the raid's escrow.
	FHollowProfileState State;
	State.Profile = Profile;
	const FHollowItemCatalog Catalog;
	const FGuid Raid = FGuid::NewGuid();
	HollowProfileRules::BeginRaid(State, Raid, FDateTime(2026, 11, 25, 12, 0, 0), Catalog);
	HollowProfileRules::MarkRadioHeard(State.Profile, TEXT("Sortie.Cathedral"));
	FHollowRaidResult Result;
	Result.RaidId = Raid;
	Result.Outcome = EHollowRaidOutcome::Dead;
	TestTrue(TEXT("The raid resolves"), HollowProfileRules::ApplyResult(State, Result, Catalog).IsSet());
	TestEqual(TEXT("Every cue heard survives the death"), State.Profile.RadioHeard.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowProfileLoreTest, "Hollow.Economy.Profile.Lore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowProfileLoreTest::RunTest(const FString& Parameters)
{
	// NAR-01b (ADR-0050): a crew log is kept the moment it is downloaded, whatever the raid's end; the bonus for one first
	// found in a raid comes home only with the player.
	const FPrimaryAssetId A(TEXT("HollowLore"), TEXT("Test_LoreA"));
	const FPrimaryAssetId B(TEXT("HollowLore"), TEXT("Test_LoreB"));
	const FPrimaryAssetId C(TEXT("HollowLore"), TEXT("Test_LoreC"));
	const FPrimaryAssetId D(TEXT("HollowLore"), TEXT("Test_LoreD"));
	constexpr int32 Bonus = 75;
	constexpr int32 Credits = 1000;
	const FHollowItemCatalog Catalog;   // no loot in these raids
	const FDateTime Now(2026, 11, 25, 12, 0, 0);
	FHollowProfileState State;
	State.Profile.Credits = Credits;

	// In the hub (a development grant): kept, no bonus waits.
	TestTrue(TEXT("A new log is kept"), HollowProfileRules::AddLore(State, A));
	TestFalse(TEXT("The same log again changes nothing"), HollowProfileRules::AddLore(State, A));
	TestFalse(TEXT("No log, nothing"), HollowProfileRules::AddLore(State, FPrimaryAssetId()));
	TestFalse(TEXT("Outside a raid no bonus waits"), State.Escrow.IsActive());

	// Extracted: the logs first found in the raid pay their bonus; one known before pays nothing.
	const FGuid Raid1 = FGuid::NewGuid();
	HollowProfileRules::BeginRaid(State, Raid1, Now, Catalog);
	TestTrue(TEXT("Found in the raid: kept at once"), HollowProfileRules::AddLore(State, B) && State.Profile.Lore.Contains(B));
	TestFalse(TEXT("Known before: not new"), HollowProfileRules::AddLore(State, A));
	TestTrue(TEXT("Its bonus waits in the escrow"), State.Escrow.NewLore == TArray<FPrimaryAssetId>{B});
	FHollowRaidResult Result;
	Result.RaidId = Raid1;
	Result.Outcome = EHollowRaidOutcome::Extracted;
	const TOptional<FHollowRaidReport> Extracted = HollowProfileRules::ApplyResult(State, Result, Catalog, 0, 0, 0, Bonus);
	TestTrue(TEXT("Extracted: +75 cr for the new log"), Extracted.IsSet() && Extracted->Delta.Lore == TArray<FPrimaryAssetId>{B}
		&& Extracted->Delta.LoreCredits == Bonus && Extracted->CreditsAfter == Credits + Bonus && State.Profile.Credits == Credits + Bonus);

	// Every other end: the log stays found, its bonus is gone.
	for (const EHollowRaidOutcome Outcome : {EHollowRaidOutcome::Dead, EHollowRaidOutcome::MissingInAction})
	{
		const int32 Before = State.Profile.Credits;
		const FGuid Raid = FGuid::NewGuid();
		const FPrimaryAssetId Entry = Outcome == EHollowRaidOutcome::Dead ? C : D;
		HollowProfileRules::BeginRaid(State, Raid, Now, Catalog);
		HollowProfileRules::AddLore(State, Entry);
		FHollowRaidResult Lost;
		Lost.RaidId = Raid;
		Lost.Outcome = Outcome;
		const TOptional<FHollowRaidReport> Report = HollowProfileRules::ApplyResult(State, Lost, Catalog, 0, 0, 0, Bonus);
		TestTrue(FString::Printf(TEXT("%s: the log stays, no bonus"), *UEnum::GetValueAsString(Outcome)), Report.IsSet() && Report->Delta.LoreCredits == 0
			&& Report->Delta.Lore == TArray<FPrimaryAssetId>{Entry} && State.Profile.Lore.Contains(Entry) && State.Profile.Credits == Before);
	}
	for (const EHollowRaidOutcome Outcome : {EHollowRaidOutcome::SignalLost, EHollowRaidOutcome::Abandoned})
	{
		const int32 Before = State.Profile.Credits;
		HollowProfileRules::BeginRaid(State, FGuid::NewGuid(), Now, Catalog);
		const FPrimaryAssetId Entry(TEXT("HollowLore"), *FString::Printf(TEXT("Test_Lore_%s"), *UEnum::GetValueAsString(Outcome)));
		HollowProfileRules::AddLore(State, Entry);
		const TOptional<FHollowRaidReport> Report = HollowProfileRules::ResolveEscrow(State, Outcome, Now, Catalog);
		TestTrue(FString::Printf(TEXT("%s (no host's result): the log stays, no bonus"), *UEnum::GetValueAsString(Outcome)), Report.IsSet()
			&& Report->Delta.LoreCredits == 0 && State.Profile.Lore.Contains(Entry) && State.Profile.Credits == Before);
	}
	TestEqual(TEXT("Every log found is kept, in the order found"), State.Profile.Lore.Num(), 6);
	TestTrue(TEXT("Nothing waits once the raids are resolved"), !State.Escrow.IsActive() && State.Escrow.NewLore.IsEmpty());
	return true;
}

#endif
