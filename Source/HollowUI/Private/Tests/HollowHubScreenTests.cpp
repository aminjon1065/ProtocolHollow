#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Classes/HollowClassDefinition.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Crafting/HollowCraftingRecipe.h"
#include "Crafting/HollowShipUpgradeDefinition.h"
#include "Engine/AssetManager.h"
#include "Foundation/HollowButton.h"
#include "Foundation/HollowPrimaryLayout.h"
#include "Foundation/HollowUISettings.h"
#include "Framework/HollowGameState.h"
#include "Framework/HollowPlayerController.h"
#include "HollowUITags.h"
#include "Hub/HollowContractDefinition.h"
#include "Hub/HollowCodexWidget.h"
#include "Hub/HollowContractTerminalWidget.h"
#include "Hub/HollowHubRowWidget.h"
#include "Hub/HollowHubScreenRouter.h"
#include "Hub/HollowHubScreenRules.h"
#include "Hub/HollowHubStateComponent.h"
#include "Hub/HollowHubTerminal.h"
#include "Hub/HollowHubTerminalSubsystem.h"
#include "Hub/HollowLoadoutWidget.h"
#include "Hub/HollowTabListWidget.h"
#include "Hub/HollowWorkbenchWidget.h"
#include "ICommonInputModule.h"
#include "Items/HollowItemDefinition.h"
#include "Lore/HollowLoreEntryDefinition.h"
#include "Profile/HollowEconomySettings.h"
#include "Profile/HollowProfileSource.h"
#include "Profile/HollowProfileTypes.h"
#include "Raid/HollowRaidLauncher.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/HollowGameTestUtils.h"
#include "Tests/HollowLocalizationTestUtils.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace HollowHubScreenTests
{
	using R = EHollowTransactionResult;

	// Hand-made terms (Hollow.UI.Hub.Rules): invented ids, never loaded.
	const FPrimaryAssetId Scrap(TEXT("HollowItem"), TEXT("Test_Scrap"));
	const FPrimaryAssetId Cell(TEXT("HollowItem"), TEXT("Test_Cell"));
	const FPrimaryAssetId Cutter(TEXT("HollowItem"), TEXT("Test_Cutter"));
	const FPrimaryAssetId Welder(TEXT("HollowItem"), TEXT("Test_Welder"));
	const FPrimaryAssetId CellRecipe(TEXT("HollowRecipe"), TEXT("Test_CellRecipe"));
	const FPrimaryAssetId Rider(TEXT("HollowUpgrade"), TEXT("Test_Rider"));
	const FPrimaryAssetId Alpha(TEXT("HollowContract"), TEXT("Test_Alpha"));
	const FPrimaryAssetId Beta(TEXT("HollowContract"), TEXT("Test_Beta"));
	const FPrimaryAssetId FirstClass(TEXT("HollowClass"), TEXT("Test_Breacher"));
	const FPrimaryAssetId SecondClass(TEXT("HollowClass"), TEXT("Test_Seer"));
	constexpr int32 ScrapValue = 8;
	constexpr int32 CellValue = 24;
	constexpr int32 CutterValue = 400;
	constexpr int32 WelderValue = 300;
	constexpr int32 ScrapStack = 50;
	constexpr int32 CellStack = 10;
	constexpr int32 ScrapPerCell = 2;
	constexpr int32 CellCredits = 5;
	constexpr int32 RiderPrice = 100;
	constexpr int32 BulkCount = 10;
	constexpr int32 SellPercent = 50;
	constexpr int32 InsurancePercent = 30;
	constexpr int32 MaxInsured = 1;
	constexpr int32 StashSlots = 4;
	constexpr int32 LoadoutSlots = 4;

	// The project's content (create_inventory_assets.py, create_economy_assets.py, create_hub_assets.py).
	const FPrimaryAssetId RealScrap(UHollowItemDefinition::AssetType, TEXT("DA_Item_Scrap"));
	const FPrimaryAssetId RealShard(UHollowItemDefinition::AssetType, TEXT("DA_Item_AetherShard"));
	const FPrimaryAssetId RealCell(UHollowItemDefinition::AssetType, TEXT("DA_Item_FuelCell"));
	const FPrimaryAssetId RealCutter(UHollowItemDefinition::AssetType, TEXT("DA_Item_PlasmaCutter"));
	const FPrimaryAssetId RealCellRecipe(UHollowCraftingRecipe::AssetType, TEXT("DA_Recipe_FuelCell"));
	const FPrimaryAssetId RealReserve(UHollowShipUpgradeDefinition::AssetType, TEXT("DA_Upgrade_FuelReserve"));
	const FPrimaryAssetId Docks(UHollowContractDefinition::AssetType, TEXT("DA_Contract_Docks"));
	const FPrimaryAssetId RealCathedral(UHollowContractDefinition::AssetType, TEXT("DA_Contract_Cathedral"));
	const FPrimaryAssetId RealCore(UHollowItemDefinition::AssetType, TEXT("DA_Item_AetherCore"));
	const FPrimaryAssetId RealPurified(UHollowItemDefinition::AssetType, TEXT("DA_Item_PurifiedCore"));
	const FPrimaryAssetId RealCoreBonus(UHollowShipUpgradeDefinition::AssetType, TEXT("DA_Upgrade_CoreBonus"));
	constexpr int32 PercentBase = 100;
	const FPrimaryAssetId RealEngineer(UHollowClassDefinition::AssetType, TEXT("DA_Class_Engineer"));
	const FPrimaryAssetId RealOccultist(UHollowClassDefinition::AssetType, TEXT("DA_Class_Occultist"));
	constexpr int32 StartCredits = 1000;
	constexpr int32 StartScrap = 6;
	constexpr int32 LockerCredits = 500;
	constexpr int32 LockerCells = 3;
	constexpr int32 LockerScrap = 2;

	FHollowItemStack Stack(const FPrimaryAssetId& Item, const int32 Count)
	{
		FHollowItemStack Result;
		Result.Item = Item;
		Result.Count = Count;
		return Result;
	}

	FHollowLoadoutSlot Slot(const FPrimaryAssetId& Item, const int32 Count, const bool bInsured = false)
	{
		FHollowLoadoutSlot Result;
		Result.Item = Item;
		Result.Count = Count;
		Result.bInsured = bInsured;
		return Result;
	}

	FHollowEconomyTerms MakeTerms()
	{
		FHollowEconomyTerms Terms;
		Terms.Catalog.Add(Scrap, ScrapValue, /*bInsurable=*/false, ScrapStack, HollowTags::Item_Type_Material);
		Terms.Catalog.Add(Cell, CellValue, /*bInsurable=*/false, CellStack, HollowTags::Item_Type_Consumable);
		Terms.Catalog.Add(Cutter, CutterValue, /*bInsurable=*/true, 1, HollowTags::Item_Type_Tool);
		Terms.Catalog.Add(Welder, WelderValue, /*bInsurable=*/true, 1, HollowTags::Item_Type_Tool);
		Terms.ShopPrices = {{Cutter, CutterValue}, {Scrap, ScrapValue}};
		FHollowRecipeTerms Recipe;
		Recipe.Id = CellRecipe;
		Recipe.Inputs = {Stack(Scrap, ScrapPerCell)};
		Recipe.Credits = CellCredits;
		Recipe.Output = Stack(Cell, 1);
		Terms.Recipes.Add(CellRecipe, Recipe);
		FHollowUpgradeTerms Upgrade;
		Upgrade.Id = Rider;
		Upgrade.Price = RiderPrice;
		Upgrade.ExtraInsuredSlots = 1;
		Terms.Upgrades.Add(Rider, Upgrade);
		Terms.SellPercent = SellPercent;
		Terms.InsurancePercent = InsurancePercent;
		Terms.MaxInsuredSlots = MaxInsured;
		Terms.StashSlots = StashSlots;
		Terms.MaxLoadoutSlots = LoadoutSlots;
		Terms.LoadoutItemTypes.AddTag(HollowTags::Item_Type_Tool);
		Terms.LoadoutItemTypes.AddTag(HollowTags::Item_Type_Consumable);
		return Terms;
	}

	/** The row with an action Predicate accepts (rows carry no ids of their own), and that action's index. */
	template <typename PredicateType>
	const FHollowHubRow* FindRow(const TArray<FHollowHubRow>& Rows, PredicateType Predicate, int32* OutAction = nullptr)
	{
		for (const FHollowHubRow& Row : Rows)
		{
			for (int32 Index = 0; Index < Row.Actions.Num(); ++Index)
			{
				if (Predicate(Row.Actions[Index]))
				{
					if (OutAction)
					{
						*OutAction = Index;
					}
					return &Row;
				}
			}
		}
		return nullptr;
	}

	/** The same among a screen's row widgets. */
	template <typename PredicateType>
	UHollowHubRowWidget* FindRowWidget(const UHollowHubScreenWidget& Screen, PredicateType Predicate, int32& OutAction)
	{
		for (UHollowHubRowWidget* Row : Screen.GetRowWidgets())
		{
			const TArray<FHollowHubAction>& Actions = Row->GetRow().Actions;
			for (int32 Index = 0; Index < Actions.Num(); ++Index)
			{
				if (Predicate(Actions[Index]))
				{
					OutAction = Index;
					return Row;
				}
			}
		}
		OutAction = INDEX_NONE;
		return nullptr;
	}

	auto Is(const EHollowTransactionKind Kind, const FPrimaryAssetId& Id)
	{
		return [Kind, Id](const FHollowHubAction& Action)
		{
			const FHollowProfileTransaction& Transaction = Action.Transaction;
			return Action.Command == EHollowHubCommand::Transaction && Transaction.Kind == Kind
				&& (Transaction.Item == Id || Transaction.Recipe == Id || Transaction.Upgrade == Id || Transaction.ClassId == Id);
		};
	}

	auto IsSlot(const EHollowTransactionKind Kind, const int32 SlotIndex)
	{
		return [Kind, SlotIndex](const FHollowHubAction& Action)
		{
			return Action.Command == EHollowHubCommand::Transaction && Action.Transaction.Kind == Kind && Action.Transaction.Slot == SlotIndex;
		};
	}

	/** What the button's press does (CommonUI broadcasts OnClicked for an interactable button). */
	void Press(UHollowHubRowWidget* Row, const int32 Action)
	{
		if (Row && Action != INDEX_NONE)
		{
			Row->GetActionButton(Action)->OnClicked().Broadcast();
		}
	}

	/** A profile in memory, applied by the same rules as the player's own (never the real one in tests). */
	class FMemoryProfile final : public IHollowProfileSource
	{
	public:
		FHollowProfileState State;

		virtual const FHollowProfileState& GetProfileState() const override { return State; }

		virtual EHollowTransactionResult ApplyTransaction(const FHollowProfileTransaction& Transaction) override
		{
			const EHollowTransactionResult Result = HollowCraftingRules::Apply(State, Transaction, FHollowEconomyTerms::Load(State, &Transaction));
			if (Result == R::Applied)
			{
				Changed.Broadcast();
			}
			return Result;
		}

		virtual FOnProfileChanged& ProfileChanged() override { return Changed; }

	private:
		FOnProfileChanged Changed;
	};

	/** A test world with the real layout (world-owned: test controllers have no ULocalPlayer). */
	struct FScreenRig
	{
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		UHollowPrimaryLayout* Layout = nullptr;
		TSharedPtr<SWidget> LayoutSlate;

		bool Build(FAutomationTestBase& Test)
		{
			World = HollowGameTests::CreateWorld(Test, TestWorld);
			const TSubclassOf<UHollowPrimaryLayout> LayoutClass = GetDefault<UHollowUISettings>()->PrimaryLayoutClass.LoadSynchronous();
			if (!World || !Test.TestNotNull(TEXT("W_PrimaryLayout"), LayoutClass.Get()))
			{
				return false;
			}
			ICommonInputModule::GetSettings().LoadData();
			Layout = CreateWidget<UHollowPrimaryLayout>(World, LayoutClass);
			if (!Test.TestNotNull(TEXT("Layout"), Layout))
			{
				return false;
			}
			LayoutSlate = Layout->TakeWidget();   // the stacks' switchers live as long as this
			return true;
		}

		UCommonActivatableWidgetContainerBase* MenuLayer() const { return Layout->GetLayer(HollowUITags::UI_Layer_Menu); }

		/** A screen of Class on the Menu layer for Player, on Profile (none: no profile), shown. */
		template <typename ScreenType>
		ScreenType* Show(const TSoftClassPtr<UHollowHubScreenWidget>& Class, IHollowProfileSource* Profile, APlayerController* Player = nullptr)
		{
			const TSubclassOf<UHollowHubScreenWidget> Loaded = Class.LoadSynchronous();
			return Loaded ? MenuLayer()->AddWidget<ScreenType>(Loaded, [Profile, Player](ScreenType& Screen)
			{
				Screen.SetProfileSourceForTests(Profile);
				Screen.OpenFor(Player, nullptr);
			}) : nullptr;
		}

		~FScreenRig()
		{
			if (Layout)
			{
				Layout->RemoveFromParent();
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHubScreenRulesTest, "Hollow.UI.Hub.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHubScreenRulesTest::RunTest(const FString& Parameters)
{
	// What the hub's screens show and allow, on hand-made terms: every button previewed by the economy's own rules.
	using namespace HollowHubScreenTests;
	const FHollowEconomyTerms Terms = MakeTerms();
	FHollowHubNames Names;
	Names.Add(Scrap, FText::FromString(TEXT("Scrap")));
	Names.Add(Cell, FText::FromString(TEXT("Cell")));
	Names.Add(Cutter, FText::FromString(TEXT("Cutter")));
	Names.Add(Rider, FText::FromString(TEXT("Rider")), FText::FromString(TEXT("One more insured slot.")));
	Names.Add(Alpha, FText::FromString(TEXT("Alpha")), FText::FromString(TEXT("First.")));

	// Every refusal says why; success says nothing.
	const UEnum* Results = StaticEnum<EHollowTransactionResult>();
	for (int32 Index = 0; Index < Results->NumEnums() - 1; ++Index)
	{
		const R Result = static_cast<R>(Results->GetValueByIndex(Index));
		TestEqual(FString::Printf(TEXT("%s has a text exactly when refused"), *Results->GetNameStringByIndex(Index)),
			HollowHubScreenRules::ResultText(Result).IsEmpty(), Result == R::Applied);
	}
	TestEqual(TEXT("Unknown ids show their asset name"), Names.GetName(Welder).ToString(), FString(TEXT("Test_Welder")));

	FHollowProfileState State;
	State.Profile.Credits = 12;
	State.Profile.Stash = {Stack(Scrap, 7)};
	State.Profile.Loadout = {Slot(Cutter, 1), Slot(Welder, 1), Slot(Cell, 3)};

	// Previews never change the profile; the craft maximum is the credits' (2), then the scrap's (3).
	const FHollowProfileState Before = State;
	TestEqual(TEXT("A preview of an affordable craft applies"), HollowHubScreenRules::Preview(State, FHollowProfileTransaction::Craft(CellRecipe, 2), Terms), R::Applied);
	TestTrue(TEXT("...on a copy"), State.Profile.Credits == Before.Profile.Credits && State.Profile.Stash == Before.Profile.Stash);
	TestEqual(TEXT("12 cr, 5 cr a craft: 2 at most"), HollowHubScreenRules::MaxCrafts(State, CellRecipe, Terms), 2);
	FHollowProfileState Rich = State;
	Rich.Profile.Credits = StartCredits;
	TestEqual(TEXT("Rich: 7 scrap, 2 a craft: 3 at most"), HollowHubScreenRules::MaxCrafts(Rich, CellRecipe, Terms), 3);
	TestEqual(TEXT("An unknown recipe: none"), HollowHubScreenRules::MaxCrafts(State, Rider, Terms), 0);

	// Craft: Craft and Craft max, greyed out with the rules' reason once the scrap is gone.
	TArray<FHollowHubRow> Rows = HollowHubScreenRules::CraftRows(State, Terms, Names);
	TestTrue(TEXT("A recipe row with Craft and Craft max (2)"), Rows.Num() == 1 && Rows[0].Actions.Num() == 2 && Rows[0].Actions[0].bAvailable
		&& Rows[0].Actions[1].Transaction.Count == 2 && Rows[0].Actions[1].bAvailable);
	FHollowProfileState NoScrap = State;
	NoScrap.Profile.Stash.Reset();
	Rows = HollowHubScreenRules::CraftRows(NoScrap, Terms, Names);
	TestTrue(TEXT("No scrap: both greyed out, \"not enough\""), Rows.Num() == 1 && !Rows[0].Actions[0].bAvailable && !Rows[0].Actions[1].bAvailable
		&& Rows[0].Actions[0].Reason.EqualTo(HollowHubScreenRules::ResultText(R::NotEnoughItems)));

	// Shop: BUY (id order), SELL (the stash); what costs too much is greyed out.
	Rows = HollowHubScreenRules::ShopRows(State, Terms, Names, BulkCount);
	TestTrue(TEXT("BUY, Cutter, Scrap, SELL, Scrap"), Rows.Num() == 5 && Rows[0].bHeader && Rows[3].bHeader && !Rows[1].bHeader
		&& Rows[1].Name.ToString() == TEXT("Cutter") && Rows[2].Name.ToString() == TEXT("Scrap") && Rows[4].Name.ToString() == TEXT("Scrap"));
	int32 Action = INDEX_NONE;
	const FHollowHubRow* Row = FindRow(Rows, Is(EHollowTransactionKind::Buy, Cutter), &Action);
	TestTrue(TEXT("12 cr: no cutter"), Row && !Row->Actions[Action].bAvailable && Row->Actions[Action].Reason.EqualTo(HollowHubScreenRules::ResultText(R::NotEnoughCredits)));
	Row = FindRow(Rows, [](const FHollowHubAction& Each) { return Each.Transaction.Kind == EHollowTransactionKind::Buy && Each.Transaction.Count == BulkCount; });
	TestTrue(TEXT("Buy N buys the settings' count"), Row != nullptr);
	Row = FindRow(Rows, Is(EHollowTransactionKind::Sell, Scrap), &Action);
	TestTrue(TEXT("Sell 1 and Sell all (7)"), Row && Row->Actions.Num() == 2 && Row->Actions[1].Transaction.Count == 7 && Row->Actions[1].bAvailable);
	TestTrue(TEXT("An empty stash sells nothing (a note, no button)"), HollowHubScreenRules::ShopRows(NoScrap, Terms, Names, BulkCount).Last().Actions.IsEmpty());

	// Upgrades: Buy, then Owned (greyed out, highlighted).
	Rows = HollowHubScreenRules::UpgradeRows(Rich, Terms, Names);
	TestTrue(TEXT("Rider for sale"), Rows.Num() == 1 && Rows[0].Actions[0].bAvailable && !Rows[0].bHighlighted && !Rows[0].Detail.IsEmpty());
	FHollowProfileState Owner = Rich;
	Owner.Profile.Upgrades = {Rider};
	Rows = HollowHubScreenRules::UpgradeRows(Owner, Terms, Names);
	TestTrue(TEXT("Owned: highlighted, greyed out"), Rows.Num() == 1 && Rows[0].bHighlighted && !Rows[0].Actions[0].bAvailable
		&& Rows[0].Actions[0].Reason.EqualTo(HollowHubScreenRules::ResultText(R::AlreadyOwned)));

	// Locker: materials stay out of raids; Return 1 only on a stack; the insurance limit, raised by the rider.
	FHollowProfileState Locker = Rich;
	Locker.Profile.Stash = {Stack(Scrap, 2), Stack(Cell, 2)};
	Rows = HollowHubScreenRules::StashRows(Locker, Terms, Names);
	Row = FindRow(Rows, Is(EHollowTransactionKind::Equip, Scrap), &Action);
	TestTrue(TEXT("Scrap: not for raids"), Row && !Row->Actions[Action].bAvailable && Row->Actions[Action].Reason.EqualTo(HollowHubScreenRules::ResultText(R::NotForRaids)));
	Row = FindRow(Rows, Is(EHollowTransactionKind::Equip, Cell), &Action);
	TestTrue(TEXT("Cells: Take 1, Take all (2)"), Row && Row->Actions.Num() == 2 && Row->Actions[0].bAvailable && Row->Actions[1].Transaction.Count == 2);
	Rows = HollowHubScreenRules::LoadoutRows(Locker, Terms, Names);
	TestTrue(TEXT("Cutter and welder: Return all, Insure; cells: Return 1 too"), Rows.Num() == 3 && Rows[0].Actions.Num() == 2 && Rows[1].Actions.Num() == 2
		&& Rows[2].Actions.Num() == 3 && Rows[2].Actions[0].Transaction.Count == 1);
	TestTrue(TEXT("Cells cannot be insured"), !Rows[2].Actions.Last().bAvailable && Rows[2].Actions.Last().Reason.EqualTo(HollowHubScreenRules::ResultText(R::NotInsurable)));
	Locker.Profile.Loadout[0].bInsured = true;
	Rows = HollowHubScreenRules::LoadoutRows(Locker, Terms, Names);
	TestTrue(TEXT("One insured of 1: the welder must wait"), Rows[0].bHighlighted && !Rows[0].Actions.Last().bAvailable && !Rows[1].Actions.Last().bAvailable
		&& Rows[1].Actions.Last().Reason.EqualTo(HollowHubScreenRules::ResultText(R::InsuranceLimit)));
	Locker.Profile.Upgrades = {Rider};
	Rows = HollowHubScreenRules::LoadoutRows(Locker, Terms, Names);
	TestTrue(TEXT("The rider raises the limit: the welder may be insured"), Rows[1].Actions.Last().bAvailable);
	TestTrue(TEXT("The status line counts against the raised limit"), HollowHubScreenRules::StatusLine(Locker, Terms).ToString().Contains(TEXT("Insured 1/2")));

	// A raid in escrow: every button waits for its result.
	FHollowProfileState InRaid = Rich;
	InRaid.Escrow.RaidId = FGuid::NewGuid();
	Rows = HollowHubScreenRules::ShopRows(InRaid, Terms, Names, BulkCount);
	Row = FindRow(Rows, Is(EHollowTransactionKind::Buy, Scrap), &Action);
	TestTrue(TEXT("In a raid: greyed out, \"raid under way\""), Row && !Row->Actions[Action].bAvailable && Row->Actions[Action].Reason.EqualTo(HollowHubScreenRules::ResultText(R::InRaid)));
	TestTrue(TEXT("...and the status line says so"), HollowHubScreenRules::StatusLine(InRaid, Terms).ToString().Contains(HollowHubScreenRules::ResultText(R::InRaid).ToString()));

	// Classes (S6-01): Choose on each, the one played highlighted and greyed out; an old profile plays the default.
	FHollowEconomyTerms ClassTerms = MakeTerms();
	FHollowClassTerms First;
	First.Id = FirstClass;
	FHollowClassTerms Second;
	Second.Id = SecondClass;
	ClassTerms.Classes.Add(FirstClass, First);
	ClassTerms.Classes.Add(SecondClass, Second);
	ClassTerms.DefaultClass = FirstClass;
	FHollowProfileState Classless = Rich;   // a profile from before classes
	Rows = HollowHubScreenRules::ClassRows(Classless, ClassTerms, Names);
	TestTrue(TEXT("Classes: the default one played, the other to choose"), Rows.Num() == 2 && Rows[0].bHighlighted && !Rows[0].Actions[0].bAvailable
		&& Rows[0].Actions[0].Reason.EqualTo(HollowHubScreenRules::ResultText(R::AlreadySelected)) && Rows[1].Actions[0].bAvailable
		&& Rows[1].Actions[0].Transaction.ClassId == SecondClass);

	// Contracts: the host chooses (the chosen one greyed out, highlighted), a guest only sees the host's choice; every row
	// and the role line name the sector and the reward (ADR-0041).
	FHollowContractOffer AlphaOffer;
	AlphaOffer.Contract = Alpha;
	AlphaOffer.SectorName = FText::FromString(TEXT("Sector A"));
	AlphaOffer.Rewards.Emplace(Cell, CellValue);
	FHollowContractOffer BetaOffer;
	BetaOffer.Contract = Beta;
	BetaOffer.SectorName = FText::FromString(TEXT("Sector B"));
	BetaOffer.Rewards.Emplace(Cutter, CutterValue);
	BetaOffer.Rewards.Emplace(Welder, WelderValue);
	const TArray<FHollowContractOffer> Offers = {BetaOffer, AlphaOffer};
	Rows = HollowHubScreenRules::ContractRows(Offers, Alpha, /*bHost=*/true, Names);
	TestTrue(TEXT("Host: id order, Choose on each, the chosen one greyed out"), Rows.Num() == 2 && Rows[0].Name.ToString() == TEXT("Alpha") && Rows[0].bHighlighted
		&& !Rows[0].Actions[0].bAvailable && Rows[1].Actions[0].bAvailable && Rows[1].Actions[0].Command == EHollowHubCommand::SelectContract
		&& Rows[1].Actions[0].Contract == Beta);
	const FString AlphaDetail = Rows[0].Detail.ToString();
	const FString BetaDetail = Rows[1].Detail.ToString();
	TestTrue(FString::Printf(TEXT("Each row: sector, reward, description (%s | %s)"), *AlphaDetail, *BetaDetail),
		AlphaDetail.Contains(TEXT("Sector A")) && AlphaDetail.Contains(TEXT("Cell 24 cr")) && AlphaDetail.Contains(TEXT("First."))
		&& BetaDetail.Contains(TEXT("Sector B")) && BetaDetail.Contains(TEXT("Cutter 400 cr")) && BetaDetail.Contains(TEXT("300 cr")));
	Rows = HollowHubScreenRules::ContractRows(Offers, Alpha, /*bHost=*/false, Names);
	TestTrue(TEXT("Guest: no buttons, the host's choice named"), Rows.Num() == 2 && Rows[0].Actions.IsEmpty() && Rows[1].Actions.IsEmpty()
		&& Rows[0].bHighlighted && Rows[0].Detail.ToString().Contains(TEXT("host")) && Rows[0].Detail.ToString().Contains(TEXT("Sector A")));
	const FString HostRole = HollowHubScreenRules::ContractRoleText(Offers, Alpha, /*bHost=*/true, Names).ToString();
	const FString GuestRole = HollowHubScreenRules::ContractRoleText(Offers, Beta, /*bHost=*/false, Names).ToString();
	TestTrue(FString::Printf(TEXT("Role lines: the squad's contract, sector and reward (%s | %s)"), *HostRole, *GuestRole),
		HostRole.Contains(TEXT("\nNow: Alpha - Sector A. Reward: Cell 24 cr")) && GuestRole.Contains(TEXT("The host chose:")) && GuestRole.Contains(TEXT("Sector B"))
		&& GuestRole.Contains(TEXT("Cutter 400 cr")));
	TestTrue(TEXT("Nothing chosen yet: who decides only"), !HollowHubScreenRules::ContractRoleText(Offers, FPrimaryAssetId(), /*bHost=*/false, Names).ToString().Contains(TEXT("chose")));
	FHollowContractOffer Bare;
	Bare.SectorName = FText::FromString(TEXT("Sector C"));
	TestEqual(TEXT("No reward: the sector alone"), HollowHubScreenRules::OfferText(Bare, Names).ToString(), FString(TEXT("Sector C")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHubWorkbenchScreenTest, "Hollow.UI.Hub.Workbench",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHubWorkbenchScreenTest::RunTest(const FString& Parameters)
{
	// The real W_Workbench on a profile in memory and the project's economy: crafting, the shop, ship upgrades.
	using namespace HollowHubScreenTests;
	FScreenRig Rig;
	if (!Rig.Build(*this))
	{
		return false;
	}
	FMemoryProfile Profile;
	Profile.State.Profile.Credits = StartCredits;
	Profile.State.Profile.Stash = {Stack(RealScrap, StartScrap), Stack(RealShard, 1)};
	const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(Profile.State);
	UHollowWorkbenchWidget* Screen = Rig.Show<UHollowWorkbenchWidget>(GetDefault<UHollowUISettings>()->WorkbenchScreenClass, &Profile);
	if (!TestNotNull(TEXT("W_Workbench on the Menu layer"), Screen))
	{
		return false;
	}
	const TOptional<FUIInputConfig> Input = Screen->GetDesiredInputConfig();
	TestTrue(TEXT("Shown, with menu input (the pawn stands still)"), Screen->IsActivated() && Input.IsSet() && Input->GetInputMode() == ECommonInputMode::Menu);
	TestTrue(TEXT("Three tabs, Craft first"), Screen->GetTabList()->GetTabCount() == 3 && Screen->GetTab() == EHollowWorkbenchTab::Craft
		&& Screen->GetTabList()->GetSelectedTabId() == UHollowWorkbenchWidget::TabId(EHollowWorkbenchTab::Craft));
	TestEqual(TEXT("A row per recipe"), Screen->GetRowWidgets().Num(), UHollowCraftingRecipe::LoadAll().Num());
	TestTrue(TEXT("The status line shows the credits"), Screen->GetStatus().ToString().Contains(FText::AsNumber(StartCredits).ToString()));

	// Craft a fuel cell: the profile changes, the rows follow at once.
	int32 Action = INDEX_NONE;
	UHollowHubRowWidget* Row = FindRowWidget(*Screen, Is(EHollowTransactionKind::Craft, RealCellRecipe), Action);
	TestTrue(TEXT("The gamepad starts on the first button that can do something"), Row && Screen->GetDesiredFocusTarget() == Screen->GetRowWidgets()[0]->GetActionButton(0));
	const FText MaxBefore = Row ? Row->GetRow().Actions[1].Label : FText::GetEmpty();
	Press(Row, Action);
	const FHollowRecipeTerms& CellTerms = Terms.Recipes.FindChecked(RealCellRecipe);
	TestTrue(TEXT("Crafted: a cell in the stash, its credits paid"), HollowCraftingRules::CountOf(Profile.State.Profile.Stash, RealCell) == CellTerms.Output.Count
		&& Profile.State.Profile.Credits == StartCredits - CellTerms.Credits);
	TestTrue(TEXT("...said on the message line"), Screen->GetMessage().ToString().Contains(TEXT("done")));
	TestTrue(TEXT("...and Craft max counts again"), Row && !Row->GetRow().Actions[1].Label.EqualTo(MaxBefore));

	// The shop: buy, and what costs too much is greyed out with its reason as the tooltip.
	Screen->SelectTab(EHollowWorkbenchTab::Shop);
	TestTrue(TEXT("Shop: the tab bar follows, BUY first"), Screen->GetTab() == EHollowWorkbenchTab::Shop
		&& Screen->GetTabList()->GetSelectedTabId() == UHollowWorkbenchWidget::TabId(EHollowWorkbenchTab::Shop)
		&& Screen->GetRowWidgets().Num() > 0 && Screen->GetRowWidgets()[0]->GetRow().bHeader);
	const int32 Credits = Profile.State.Profile.Credits;
	Row = FindRowWidget(*Screen, Is(EHollowTransactionKind::Buy, RealScrap), Action);
	Press(Row, Action);
	TestEqual(TEXT("Bought a scrap at the shop's price"), Profile.State.Profile.Credits, Credits - Terms.ShopPrices.FindChecked(RealScrap));
	UHollowHubRowWidget* Expensive = FindRowWidget(*Screen, [](const FHollowHubAction& Each)
	{
		return Each.Transaction.Kind == EHollowTransactionKind::Buy && Each.Transaction.Item == RealCutter && Each.Transaction.Count > 1;
	}, Action);
	const UHollowButton* Greyed = Expensive ? Expensive->GetActionButton(Action) : nullptr;
	TestTrue(TEXT("Ten cutters: greyed out, focusable, the reason as tooltip"), Greyed && !Greyed->IsInteractionEnabled() && Greyed->IsVisible()
		&& Greyed->GetToolTipText().EqualTo(HollowHubScreenRules::ResultText(EHollowTransactionResult::NotEnoughCredits)));
	TestNotNull(TEXT("The crafted cell can be sold"), FindRowWidget(*Screen, Is(EHollowTransactionKind::Sell, RealCell), Action));

	// Ship upgrades: bought once, then owned.
	Screen->SelectTab(EHollowWorkbenchTab::Upgrades);
	TestEqual(TEXT("A row per ship upgrade"), Screen->GetRowWidgets().Num(), UHollowShipUpgradeDefinition::LoadAll().Num());
	Row = FindRowWidget(*Screen, Is(EHollowTransactionKind::BuyUpgrade, RealReserve), Action);
	Press(Row, Action);
	TestTrue(TEXT("The fuel reserve is the ship's"), Profile.State.Profile.Upgrades.Contains(RealReserve));
	Row = FindRowWidget(*Screen, Is(EHollowTransactionKind::BuyUpgrade, RealReserve), Action);
	TestTrue(TEXT("...shown owned: highlighted, greyed out"), Row && Row->GetRow().bHighlighted && !Row->GetActionButton(Action)->IsInteractionEnabled());

	// Back (Esc / B) leaves it.
	Screen->NativeOnHandleBackAction();
	TestTrue(TEXT("Back closes the screen"), !Screen->IsActivated() && Rig.MenuLayer()->GetActiveWidget() == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHubLoadoutScreenTest, "Hollow.UI.Hub.Loadout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHubLoadoutScreenTest::RunTest(const FString& Parameters)
{
	// The real W_Loadout: stash to loadout and back, insurance.
	using namespace HollowHubScreenTests;
	FScreenRig Rig;
	if (!Rig.Build(*this))
	{
		return false;
	}
	FMemoryProfile Profile;
	Profile.State.Profile.Credits = LockerCredits;
	Profile.State.Profile.ClassId = GetDefault<UHollowEconomySettings>()->DefaultClass;   // the Engineer (S6-01)
	Profile.State.Profile.Loadout = {Slot(RealCutter, 1)};
	Profile.State.Profile.Stash = {Stack(RealCell, LockerCells), Stack(RealScrap, LockerScrap)};
	const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(Profile.State);
	UHollowLoadoutWidget* Screen = Rig.Show<UHollowLoadoutWidget>(GetDefault<UHollowUISettings>()->LoadoutScreenClass, &Profile);
	if (!TestNotNull(TEXT("W_Loadout on the Menu layer"), Screen))
	{
		return false;
	}
	const int32 Classes = UHollowClassDefinition::LoadAll().Num();
	TestEqual(TEXT("Two stash rows, one loadout row, a row per class"), Screen->GetRowWidgets().Num(), 3 + Classes);
	int32 Action = INDEX_NONE;
	const UHollowHubRowWidget* ScrapRow = FindRowWidget(*Screen, Is(EHollowTransactionKind::Equip, RealScrap), Action);
	TestTrue(TEXT("Scrap stays out of raids: greyed out"), ScrapRow && !ScrapRow->GetActionButton(Action)->IsInteractionEnabled());
	TestTrue(TEXT("The gamepad starts on the first button that can do something (not the scrap's)"),
		Screen->GetDesiredFocusTarget() != (ScrapRow ? ScrapRow->GetActionButton(0) : nullptr));

	// Take all the cells: a new loadout slot, their stash row goes.
	UHollowHubRowWidget* Row = FindRowWidget(*Screen, [](const FHollowHubAction& Each)
	{
		return Each.Transaction.Kind == EHollowTransactionKind::Equip && Each.Transaction.Item == RealCell && Each.Transaction.Count == LockerCells;
	}, Action);
	Press(Row, Action);
	const TArray<FHollowLoadoutSlot>& Loadout = Profile.State.Profile.Loadout;
	TestTrue(TEXT("The cells are in the loadout"), Loadout.Num() == 2 && Loadout[1].Item == RealCell && Loadout[1].Count == LockerCells);
	TestNull(TEXT("...and no longer in the stash"), FindRowWidget(*Screen, Is(EHollowTransactionKind::Equip, RealCell), Action));

	// Insure the cutter: paid, highlighted, greyed out after.
	Row = FindRowWidget(*Screen, IsSlot(EHollowTransactionKind::Insure, 0), Action);
	Press(Row, Action);
	TestTrue(TEXT("The cutter is insured for its price"), Loadout[0].bInsured
		&& Profile.State.Profile.Credits == LockerCredits - HollowCraftingRules::InsurancePrice(Terms, Slot(RealCutter, 1)));
	Row = FindRowWidget(*Screen, IsSlot(EHollowTransactionKind::Insure, 0), Action);
	TestTrue(TEXT("...shown insured"), Row && Row->GetRow().bHighlighted && !Row->GetActionButton(Action)->IsInteractionEnabled());

	// Return all the cells: back in the stash.
	Row = FindRowWidget(*Screen, [](const FHollowHubAction& Each)
	{
		return Each.Transaction.Kind == EHollowTransactionKind::Unequip && Each.Transaction.Slot == 1 && Each.Transaction.Count == 0;
	}, Action);
	Press(Row, Action);
	TestTrue(TEXT("Returned: one slot left, the cells in the stash"), Loadout.Num() == 1
		&& HollowCraftingRules::CountOf(Profile.State.Profile.Stash, RealCell) == LockerCells);
	TestEqual(TEXT("Rows: two in the stash, one in the loadout again, the classes"), Screen->GetRowWidgets().Num(), 3 + Classes);

	// The classes (S6-01, ADR-0034): the one played greyed out; another brings its own loadout, this one is kept.
	const TArray<FHollowLoadoutSlot> EngineerLoadout = Loadout;
	Row = FindRowWidget(*Screen, Is(EHollowTransactionKind::SelectClass, RealEngineer), Action);
	TestTrue(TEXT("The Engineer, played: highlighted, greyed out"), Row && Row->GetRow().bHighlighted && !Row->GetActionButton(Action)->IsInteractionEnabled());
	TestTrue(TEXT("...and no class switch is where the gamepad starts"), Screen->GetDesiredFocusTarget() != (Row ? Row->GetActionButton(Action) : nullptr));
	Row = FindRowWidget(*Screen, Is(EHollowTransactionKind::SelectClass, RealOccultist), Action);
	Press(Row, Action);
	const UHollowClassDefinition* OccultistClass = UHollowClassDefinition::Load(RealOccultist);
	TestTrue(TEXT("The Occultist with its starting loadout"), OccultistClass && Profile.State.Profile.ClassId == RealOccultist
		&& Profile.State.Profile.Loadout == OccultistClass->StartingLoadout);
	TestTrue(TEXT("...the Engineer's loadout kept for it"), Profile.State.Profile.ClassLoadouts.Num() == 1
		&& Profile.State.Profile.ClassLoadouts[0].ClassId == RealEngineer && Profile.State.Profile.ClassLoadouts[0].Loadout == EngineerLoadout);
	Row = FindRowWidget(*Screen, Is(EHollowTransactionKind::SelectClass, RealOccultist), Action);
	TestTrue(TEXT("...shown played"), Row && Row->GetRow().bHighlighted);
	Screen->DeactivateWidget();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHubContractScreenTest, "Hollow.UI.Hub.Contracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHubContractScreenTest::RunTest(const FString& Parameters)
{
	// The real W_ContractTerminal for the host: choosing goes through the server, the screen follows the hub state; it
	// shows both sectors' contracts with their reward priced for this player (their ship's core bonus, ADR-0041) and
	// follows a change of the squad's contract live, as a guest's screen does.
	using namespace HollowHubScreenTests;
	FScreenRig Rig;
	if (!Rig.Build(*this))
	{
		return false;
	}
	Rig.World->SpawnActor<AHollowGameState>();   // not begun: no contract chosen yet
	UHollowHubStateComponent* Hub = UHollowHubStateComponent::Get(Rig.World);
	AHollowPlayerController* Host = Rig.World->SpawnActor<AHollowPlayerController>();
	if (!TestNotNull(TEXT("Hub state"), Hub) || !TestNotNull(TEXT("Host"), Host))
	{
		return false;
	}
	Host->SetAsLocalPlayerController();
	FMemoryProfile Profile;
	Profile.State.Profile.Credits = StartCredits;
	Profile.State.Profile.Upgrades = {RealCoreBonus};
	UHollowContractTerminalWidget* Screen = Rig.Show<UHollowContractTerminalWidget>(GetDefault<UHollowUISettings>()->ContractScreenClass, &Profile, Host);
	if (!TestNotNull(TEXT("W_ContractTerminal on the Menu layer"), Screen))
	{
		return false;
	}
	TArray<FPrimaryAssetId> Contracts;
	UAssetManager::Get().GetPrimaryAssetIdList(UHollowContractDefinition::AssetType, Contracts);
	TestTrue(TEXT("The host's screen: a row per contract"), Screen->IsHostScreen() && Screen->GetRowWidgets().Num() == Contracts.Num());
	TestEqual(TEXT("Launch only where a raid can start (the hub; not a test world)"), Screen->GetLaunchButton()->IsVisible(), HollowRaidLauncher::CanStartRaid(Rig.World));

	int32 Action = INDEX_NONE;
	UHollowHubRowWidget* Row = FindRowWidget(*Screen, [](const FHollowHubAction& Each)
	{
		return Each.Command == EHollowHubCommand::SelectContract && Each.Contract == Docks;
	}, Action);
	TestTrue(TEXT("Docks can be chosen"), Row && Row->GetActionButton(Action)->IsInteractionEnabled() && !Row->GetRow().bHighlighted);
	Press(Row, Action);
	TestTrue(TEXT("The server made it the squad's"), Hub->GetSelectedContract() == Docks);
	Row = FindRowWidget(*Screen, [](const FHollowHubAction& Each) { return Each.Contract == Docks; }, Action);
	TestTrue(TEXT("The screen followed: chosen, highlighted, greyed out"), Row && Row->GetRow().bHighlighted && !Row->GetActionButton(Action)->IsInteractionEnabled());

	// What the Cathedral pays this player: a core and a purified core, with the ship's core bonus (both are Aether cores).
	const UHollowItemDefinition* Core = Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(RealCore).TryLoad());
	const UHollowItemDefinition* Purified = Cast<UHollowItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(RealPurified).TryLoad());
	const UHollowShipUpgradeDefinition* Bonus = Cast<UHollowShipUpgradeDefinition>(UAssetManager::Get().GetPrimaryAssetPath(RealCoreBonus).TryLoad());
	if (!TestTrue(TEXT("The core, the purified core, the core bonus"), Core && Purified && Bonus && Bonus->CoreValuePercent > 0))
	{
		return false;
	}
	// Numbers as the screen formats them (the culture's grouping: 1,500).
	const FString CoreCredits = FText::AsNumber(Core->BaseValue * (PercentBase + Bonus->CoreValuePercent) / PercentBase).ToString() + TEXT(" cr");
	const FString PurifiedCredits = FText::AsNumber(Purified->BaseValue * (PercentBase + Bonus->CoreValuePercent) / PercentBase).ToString() + TEXT(" cr");
	Row = FindRowWidget(*Screen, [](const FHollowHubAction& Each) { return Each.Contract == RealCathedral; }, Action);
	const FString Detail = Row ? Row->GetRow().Detail.ToString() : FString();
	TestTrue(FString::Printf(TEXT("The Cathedral's row: its sector, both cores at this player's price (%s; %s, %s)"), *Detail, *CoreCredits, *PurifiedCredits),
		Detail.Contains(TEXT("Reactor Cathedral")) && Detail.Contains(CoreCredits) && Detail.Contains(PurifiedCredits));

	// The squad's contract changes on the server (the host chose elsewhere): the open screen follows at once.
	Hub->SelectContract(RealCathedral);
	Row = FindRowWidget(*Screen, [](const FHollowHubAction& Each) { return Each.Contract == RealCathedral; }, Action);
	const FString Role = Screen->GetRoleText().ToString();
	TestTrue(FString::Printf(TEXT("Live: the Cathedral highlighted, the role line names it and its reward (%s)"), *Role), Row && Row->GetRow().bHighlighted
		&& Role.Contains(TEXT("Reactor Cathedral")) && Role.Contains(PurifiedCredits));
	Screen->DeactivateWidget();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowHubRoutingTest, "Hollow.UI.Hub.Routing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowHubRoutingTest::RunTest(const FString& Parameters)
{
	// Terminal -> screen: the player's own terminal shows its screen on the Menu layer; closing either closes both.
	using namespace HollowHubScreenTests;
	FScreenRig Rig;
	if (!Rig.Build(*this))
	{
		return false;
	}
	AHollowPlayerController* Host = Rig.World->SpawnActor<AHollowPlayerController>();
	AHollowPlayerController* Guest = Rig.World->SpawnActor<AHollowPlayerController>();
	AHollowHubTerminal* Workbench = Rig.World->SpawnActor<AHollowHubTerminal>();   // a workbench unless set otherwise
	UHollowHubTerminalSubsystem* Terminals = UHollowHubTerminalSubsystem::Get(Rig.World);
	if (!TestTrue(TEXT("Players, a terminal and the terminals' subsystem"), Host && Guest && Workbench && Terminals))
	{
		return false;
	}
	Host->SetAsLocalPlayerController();
	TestTrue(TEXT("Each terminal kind has its screen"),
		UHollowHubScreenRouter::ScreenClass(EHollowTerminalKind::Workbench)->IsChildOf(UHollowWorkbenchWidget::StaticClass())
		&& UHollowHubScreenRouter::ScreenClass(EHollowTerminalKind::Locker)->IsChildOf(UHollowLoadoutWidget::StaticClass())
		&& UHollowHubScreenRouter::ScreenClass(EHollowTerminalKind::Contracts)->IsChildOf(UHollowContractTerminalWidget::StaticClass())
		&& UHollowHubScreenRouter::ScreenClass(EHollowTerminalKind::Codex)->IsChildOf(UHollowCodexWidget::StaticClass()));
	UHollowHubScreenRouter* Router = NewObject<UHollowHubScreenRouter>(GetTransientPackage());
	Router->Initialize(Rig.Layout, Host);

	Terminals->Open(Guest, Workbench);
	TestNull(TEXT("Another player's terminal shows nothing here"), Router->GetScreen());
	Terminals->Open(Host, Workbench);
	UHollowHubScreenWidget* Screen = Router->GetScreen();
	TestTrue(TEXT("The host's workbench: W_Workbench on the Menu layer, for the host at that terminal"), Screen && Screen->IsA<UHollowWorkbenchWidget>()
		&& Rig.MenuLayer()->GetActiveWidget() == Screen && Screen->GetHubPlayer() == Host && Screen->GetTerminal() == Workbench);
	Terminals->Close(Host);
	TestTrue(TEXT("The terminal closed elsewhere (Hollow.Hub.Close): its screen goes"), Screen && !Screen->IsActivated() && !Router->GetScreen());

	Terminals->Open(Host, Workbench);
	Screen = Router->GetScreen();
	if (TestNotNull(TEXT("Opened again"), Screen))
	{
		Screen->NativeOnHandleBackAction();
		TestTrue(TEXT("Back: the screen goes and the terminal closes"), !Screen->IsActivated() && !Terminals->GetOpenTerminal(Host)
			&& Rig.MenuLayer()->GetActiveWidget() == nullptr);
	}
	TestTrue(TEXT("The guest's terminal stays open"), Terminals->GetOpenTerminal(Guest) == Workbench);
	Router->Deinitialize();
	return true;
}

namespace HollowHubScreenTests
{
	/** Whether Text has a Latin word (a text to translate); numbers, signs and Roman numerals (Sector II) read the same. */
	bool HasLatin(const FString& Text)
	{
		FString Word;
		const auto IsLatinWord = [](const FString& Each)
		{
			bool bLatin = false;
			bool bRoman = true;
			for (const TCHAR Character : Each)
			{
				bLatin |= (Character >= TCHAR('A') && Character <= TCHAR('Z')) || (Character >= TCHAR('a') && Character <= TCHAR('z'));
				bRoman &= FCString::Strchr(TEXT("IVXLCDM"), Character) != nullptr;
			}
			return bLatin && !bRoman;
		};
		for (const TCHAR Character : Text + TEXT(" "))
		{
			if (FChar::IsAlpha(Character))
			{
				Word.AppendChar(Character);
				continue;
			}
			if (IsLatinWord(Word))
			{
				return true;
			}
			Word.Reset();
		}
		return false;
	}

	/**
	 * LOC-01: the shown screen switched to Russian and back: every text with a Latin letter reads Russian (the same
	 * widgets, no rebuild), nothing is cut on a 1080p screen in either language, English comes back the same.
	 */
	void CheckLanguages(FAutomationTestBase& Test, UUserWidget& Screen, const FString& Name)
	{
		using namespace HollowLocalizationTests;
		const FVector2D View(1920.0, 1080.0);
		for (const FString& Overflow : FindOverflows(Screen, View))   // lays it out first: every label is set
		{
			Test.AddError(FString::Printf(TEXT("%s, cut in English: %s"), *Name, *Overflow));
		}
		const TMap<FString, FString> English = ReadTexts(Screen);
		Test.TestTrue(FString::Printf(TEXT("%s: texts to read"), *Name), English.Num() > 2);
		{
			const FScopedLanguage Russian(EHollowLanguage::Russian);
			const TMap<FString, FString> Now = ReadTexts(Screen);
			for (const TPair<FString, FString>& Text : English)
			{
				const FString* Shown = Now.Find(Text.Key);
				if (HasLatin(Text.Value) && !(Shown && HasCyrillic(*Shown) && !HasLatin(*Shown)))
				{
					Test.AddError(FString::Printf(TEXT("%s: %s \"%s\" reads \"%s\" in Russian"), *Name, *Text.Key, *Text.Value, Shown ? **Shown : TEXT("-")));
				}
			}
			for (const FString& Overflow : FindOverflows(Screen, View))
			{
				Test.AddError(FString::Printf(TEXT("%s, cut in Russian: %s"), *Name, *Overflow));
			}
		}
		for (const FString& Change : Differences(English, ReadTexts(Screen)))
		{
			Test.AddError(FString::Printf(TEXT("%s, not back in English: %s"), *Name, *Change));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCodexScreenTest, "Hollow.UI.Codex.Screen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCodexScreenTest::RunTest(const FString& Parameters)
{
	// NAR-01b (ADR-0050): the real W_Codex on a profile that found the first log of each sector: the twelve logs by
	// sector with each sector's count and the total; a found log reads in full, one not found shows encrypted and
	// cannot be read; the gamepad starts on the first log found; then every log, found, reads whole in its reader on a
	// 1080p screen in both languages.
	using namespace HollowHubScreenTests;
	FScreenRig Rig;
	const TArray<const UHollowLoreEntryDefinition*> Entries = UHollowLoreEntryDefinition::LoadAll();
	if (!TestTrue(TEXT("Slate measures text here"), HollowLocalizationTests::CanMeasureText()) || !TestEqual(TEXT("Twelve logs"), Entries.Num(), 12)
		|| !Rig.Build(*this))
	{
		return false;
	}
	const UHollowLoreEntryDefinition* DocksFirst = Entries[0];
	const UHollowLoreEntryDefinition* DocksSecond = Entries[1];
	const UHollowLoreEntryDefinition* CathedralFirst = Entries[6];
	FMemoryProfile Profile;
	Profile.State.Profile.Lore = {CathedralFirst->GetPrimaryAssetId(), DocksFirst->GetPrimaryAssetId()};
	UHollowCodexWidget* Codex = Rig.Show<UHollowCodexWidget>(GetDefault<UHollowUISettings>()->CodexScreenClass, &Profile);
	if (!TestNotNull(TEXT("W_Codex on the Menu layer"), Codex))
	{
		return false;
	}
	TestEqual(TEXT("Two sector headers and twelve logs"), Codex->GetRowWidgets().Num(), 14);
	TestTrue(TEXT("Found 2 of 12"), Codex->GetFoundCount() == 2 && Codex->GetTotalCount() == 12);
	TestTrue(TEXT("The status line says it"), Codex->GetStatus().ToString().Contains(TEXT("2/12")));
	const FText DocksHeader = Codex->GetRowWidgets()[0]->GetRow().Name;
	TestTrue(TEXT("Each sector's header: its name and found count"), Codex->GetRowWidgets()[0]->GetRow().bHeader
		&& DocksHeader.ToString().Contains(DocksFirst->SectorName.ToString()) && DocksHeader.ToString().Contains(TEXT("1/6")));
	TestTrue(TEXT("The reader opens on the first log found, whole"), Codex->GetReading() == DocksFirst->GetPrimaryAssetId()
		&& Codex->GetReaderTitle().EqualTo(DocksFirst->Title) && Codex->GetReaderBody().EqualTo(DocksFirst->Body)
		&& Codex->GetReaderMeta().ToString().Contains(DocksFirst->Author.ToString()) && Codex->GetReaderMeta().ToString().Contains(DocksFirst->Stamp.ToString()));
	int32 Action = INDEX_NONE;
	const auto Reads = [](const FPrimaryAssetId& Id) { return [Id](const FHollowHubAction& Each) { return Each.Command == EHollowHubCommand::ReadLore && Each.Lore == Id; }; };
	UHollowHubRowWidget* Hidden = FindRowWidget(*Codex, Reads(DocksSecond->GetPrimaryAssetId()), Action);
	TestTrue(TEXT("A log not found: [DATA ENCRYPTED - its sector], its Read greyed out"), Hidden && Hidden->GetRow().Name.ToString().Contains(DocksSecond->SectorName.ToString())
		&& !Hidden->GetRow().Name.ToString().Contains(DocksSecond->Title.ToString()) && !Hidden->GetActionButton(Action)->IsInteractionEnabled());
	TestTrue(TEXT("The gamepad starts on the first log found"), Codex->GetDesiredFocusTarget() == FindRowWidget(*Codex, Reads(DocksFirst->GetPrimaryAssetId()), Action)->GetActionButton(Action));
	UHollowHubRowWidget* Found = FindRowWidget(*Codex, Reads(CathedralFirst->GetPrimaryAssetId()), Action);
	Press(Found, Action);
	TestTrue(TEXT("Read: the Cathedral's log in the reader"), Codex->GetReaderTitle().EqualTo(CathedralFirst->Title) && Codex->GetReaderBody().EqualTo(CathedralFirst->Body));
	Codex->Read(DocksSecond->GetPrimaryAssetId());
	TestTrue(TEXT("A log not found reads encrypted"), Codex->GetReaderTitle().ToString().Contains(DocksSecond->SectorName.ToString())
		&& !Codex->GetReaderBody().EqualTo(DocksSecond->Body) && Codex->GetReaderMeta().IsEmpty());

	// Every log found: each reads whole in the reader at 1080p, in English and in Russian (no scrolling).
	for (const UHollowLoreEntryDefinition* Entry : Entries)
	{
		Profile.State.Profile.Lore.AddUnique(Entry->GetPrimaryAssetId());
	}
	Codex->Refresh();
	TestEqual(TEXT("All found: 12/12"), Codex->GetFoundCount(), 12);
	UWidget* Reader = Codex->WidgetTree->FindWidget(TEXT("Reader"));
	UWidget* ReaderBox = Codex->WidgetTree->FindWidget(TEXT("ReaderBox"));
	const UPanelWidget* List = Cast<UPanelWidget>(Codex->WidgetTree->FindWidget(TEXT("EntryList")));
	if (!TestTrue(TEXT("The reader and the list"), Reader && ReaderBox && List))
	{
		return false;
	}
	const FVector2D View(1920.0, 1080.0);
	for (const EHollowLanguage Language : {EHollowLanguage::English, EHollowLanguage::Russian})
	{
		const HollowLocalizationTests::FScopedLanguage Scoped(Language);
		for (const UHollowLoreEntryDefinition* Entry : Entries)
		{
			Codex->Read(Entry->GetPrimaryAssetId());
			for (const FString& Overflow : HollowLocalizationTests::FindOverflows(*Codex, View))
			{
				AddError(FString::Printf(TEXT("%s, %s: cut: %s"), *Entry->GetName(), LexToString(Language), *Overflow));
			}
			const double Height = Reader->GetDesiredSize().Y;
			const double Room = ReaderBox->GetDesiredSize().Y;
			TestTrue(*FString::Printf(TEXT("%s, %s: the whole log reads (%.0f of %.0f px)"), *Entry->GetName(), LexToString(Language), Height, Room), Height > 0.0 && Height <= Room);
		}
	}
	Codex->NativeOnHandleBackAction();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLocalizationHubTest, "Hollow.UI.Localization.Hub",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLocalizationHubTest::RunTest(const FString& Parameters)
{
	// LOC-01 (ADR-0048): the real hub screens with real data (the workbench's three tabs, the locker, the Codex of
	// NAR-01b, the contracts) read Russian at once when the language changes, and their columns and rows hold the longer
	// Russian words.
	using namespace HollowHubScreenTests;
	FScreenRig Rig;
	if (!TestTrue(TEXT("Slate measures text here"), HollowLocalizationTests::CanMeasureText()) || !Rig.Build(*this))
	{
		return false;
	}
	FMemoryProfile Profile;
	Profile.State.Profile.Credits = StartCredits;
	Profile.State.Profile.ClassId = GetDefault<UHollowEconomySettings>()->DefaultClass;
	Profile.State.Profile.Loadout = {Slot(RealCutter, 1)};
	Profile.State.Profile.Stash = {Stack(RealScrap, StartScrap), Stack(RealShard, 1), Stack(RealCell, 1)};
	UHollowWorkbenchWidget* Workbench = Rig.Show<UHollowWorkbenchWidget>(GetDefault<UHollowUISettings>()->WorkbenchScreenClass, &Profile);
	if (!TestNotNull(TEXT("W_Workbench"), Workbench))
	{
		return false;
	}
	for (const EHollowWorkbenchTab Tab : {EHollowWorkbenchTab::Craft, EHollowWorkbenchTab::Shop, EHollowWorkbenchTab::Upgrades})
	{
		Workbench->SelectTab(Tab);
		CheckLanguages(*this, *Workbench, FString::Printf(TEXT("W_Workbench %s"), *StaticEnum<EHollowWorkbenchTab>()->GetNameStringByValue(static_cast<int64>(Tab))));
	}
	Workbench->NativeOnHandleBackAction();

	UHollowLoadoutWidget* Locker = Rig.Show<UHollowLoadoutWidget>(GetDefault<UHollowUISettings>()->LoadoutScreenClass, &Profile);
	if (TestNotNull(TEXT("W_Loadout"), Locker))
	{
		CheckLanguages(*this, *Locker, TEXT("W_Loadout"));
		Locker->NativeOnHandleBackAction();
	}

	// NAR-01b: the Codex with a log found in each sector (the rest encrypted).
	const TArray<const UHollowLoreEntryDefinition*> Entries = UHollowLoreEntryDefinition::LoadAll();
	if (TestEqual(TEXT("Twelve crew logs"), Entries.Num(), 12))
	{
		FMemoryProfile Archive;
		Archive.State.Profile.Lore = {Entries[0]->GetPrimaryAssetId(), Entries[6]->GetPrimaryAssetId()};
		UHollowCodexWidget* Codex = Rig.Show<UHollowCodexWidget>(GetDefault<UHollowUISettings>()->CodexScreenClass, &Archive);
		if (TestNotNull(TEXT("W_Codex"), Codex))
		{
			CheckLanguages(*this, *Codex, TEXT("W_Codex"));
			Codex->NativeOnHandleBackAction();
		}
	}

	Rig.World->SpawnActor<AHollowGameState>();
	AHollowPlayerController* Host = Rig.World->SpawnActor<AHollowPlayerController>();
	if (TestNotNull(TEXT("Host"), Host))
	{
		Host->SetAsLocalPlayerController();
		UHollowContractTerminalWidget* Contracts = Rig.Show<UHollowContractTerminalWidget>(GetDefault<UHollowUISettings>()->ContractScreenClass, &Profile, Host);
		if (TestNotNull(TEXT("W_ContractTerminal"), Contracts))
		{
			CheckLanguages(*this, *Contracts, TEXT("W_ContractTerminal"));
			Contracts->NativeOnHandleBackAction();
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
