#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarryableComponent.h"
#include "Classes/HollowClassDefinition.h"
#include "Crafting/HollowCraftingRules.h"
#include "Crafting/HollowShipUpgradeDefinition.h"
#include "Foundation/HollowUISettings.h"
#include "Framework/HollowGameState.h"
#include "GameplayEffect.h"
#include "Hub/HollowHubTerminal.h"
#include "Hub/HollowHubTerminalSubsystem.h"
#include "Profile/HollowProfileSource.h"
#include "Profile/HollowProfileTypes.h"
#include "Raid/HollowExtractionZone.h"
#include "Raid/HollowRaidStateComponent.h"
#include "Sector/HollowSectorTopology.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Settings/HollowGameUserSettings.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "ViewModels/HollowObjectiveModel.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowControlsHintSet.h"
#include "Visor/HollowControlsHintWidget.h"
#include "Visor/HollowObjectiveRules.h"
#include "Visor/HollowObjectiveWidget.h"
#include "Visor/HollowVisorStyle.h"
#include "Visor/HollowWaypointLayerWidget.h"
#include "World/HollowRitualAltar.h"
#include "World/HollowRitualAltarDefinition.h"

namespace HollowObjectiveTests
{
	// Content under test (create_visor_assets.py, create_carry_assets.py, create_raid_flow_assets.py, create_cathedral_mechanics.py).
	const TCHAR* const VisorPath = TEXT("/Game/Hollow/UI/Visor/W_Visor.W_Visor_C");
	const TCHAR* const ControlsClass = TEXT("/Game/Hollow/UI/Visor/W_ControlsHint.W_ControlsHint_C");
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const ZoneClass = TEXT("/Game/Hollow/World/Raid/BP_ExtractionZone.BP_ExtractionZone_C");
	const TCHAR* const AltarClass = TEXT("/HollowSector_Cathedral/Mechanics/BP_RitualAltar.BP_RitualAltar_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const FPrimaryAssetId CoreBonus(UHollowShipUpgradeDefinition::AssetType, TEXT("DA_Upgrade_CoreBonus"));
	const FPrimaryAssetId Engineer(UHollowClassDefinition::AssetType, TEXT("DA_Class_Engineer"));
	const FPrimaryAssetId Occultist(UHollowClassDefinition::AssetType, TEXT("DA_Class_Occultist"));

	/** Expected data (create_economy_assets.py: core 500 cr, purified 1250 cr; the core bonus +20 %). */
	constexpr int32 CoreCredits = 500;
	constexpr int32 PurifiedCredits = 1250;
	constexpr int32 BonusCoreCredits = 600;
	constexpr int32 BonusPurifiedCredits = 1500;
	constexpr int32 WaypointKinds = 6;
	constexpr int32 ControlRows = 9;   // GP-01: the hands and the melee blow

	/** Test data. */
	constexpr float TickSeconds = 1.f / 30.f;
	const FVector2D ViewSize(1920.0, 1080.0);
	constexpr float Margin = 96.f;
	constexpr float FovDegrees = 90.f;
	constexpr double CoreAhead = 600.0;
	constexpr double ZoneBehind = -3000.0;
	constexpr double AltarAhead = 250.0;
	constexpr double TerminalAhead = 800.0;
	constexpr double TerminalSide = 400.0;
	constexpr int32 DepartureSeconds = 42;
	constexpr double RoomHalfSize = 4000.0;
	constexpr double RoomHeight = 800.0;
	constexpr double Tolerance = 0.5;

	void Advance(FTestWorldWrapper& World, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;   // the timer manager ticks once per frame counter
			World.TickTestWorld(TickSeconds);
		}
	}

	/** A profile in memory (the real one never exists in automation worlds). */
	class FMemoryProfile final : public IHollowProfileSource
	{
	public:
		FHollowProfileState State;
		virtual const FHollowProfileState& GetProfileState() const override { return State; }
		virtual EHollowTransactionResult ApplyTransaction(const FHollowProfileTransaction& Transaction) override { return EHollowTransactionResult::Applied; }
		virtual FOnProfileChanged& ProfileChanged() override { return Changed; }

	private:
		FOnProfileChanged Changed;
	};

	/** The local player's ExoFrame with its real attributes, its visor context, and the game state with its raid state. */
	struct FRig
	{
		FTestWorldWrapper World;
		AHollowGameState* GameState = nullptr;
		AHollowExoFrameCharacter* Character = nullptr;
		APlayerController* Controller = nullptr;
		UHollowAbilitySystemComponent* AbilitySystem = nullptr;
		UHollowVisorContext* Context = nullptr;
		UHollowObjectiveModel* Objective = nullptr;
	};

	bool Build(FAutomationTestBase& Test, FRig& Rig)
	{
		UWorld* World = HollowGameTests::CreateWorld(Test, Rig.World);
		if (!World)
		{
			return false;
		}
		Rig.GameState = World->SpawnActor<AHollowGameState>();
		Rig.GameState->DispatchBeginPlay();
		// One room round everything, so the altar's heat has somewhere to go.
		FHollowSectorTopology Topology;
		FHollowTopologyRoom& Room = Topology.Rooms.AddDefaulted_GetRef();
		Room.Bounds = FBox(FVector(-RoomHalfSize, -RoomHalfSize, -RoomHeight), FVector(RoomHalfSize, RoomHalfSize, RoomHeight));
		if (UHollowSectorTopologySubsystem* TopologySubsystem = UHollowSectorTopologySubsystem::Get(World))
		{
			TopologySubsystem->SetTopology(Topology);
		}
		Rig.Character = HollowGameTests::SpawnExoFrame(Test, World);
		const TSubclassOf<UGameplayEffect> Init = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
		if (!Test.TestNotNull(TEXT("ExoFrame"), Rig.Character) || !Test.TestNotNull(TEXT("GE_ExoFrame_InitAttributes"), Init.Get()))
		{
			return false;
		}
		Test.AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
		Rig.Controller = HollowGameTests::PossessAsPlayer(World, Rig.Character, /*bLocalPlayer=*/true);
		Rig.Controller->SetControlRotation(FRotator::ZeroRotator);
		Rig.Character->DispatchBeginPlay();
		Rig.AbilitySystem = Rig.Character->GetHollowAbilitySystemComponent();
		if (!Test.TestNotNull(TEXT("Ability system"), Rig.AbilitySystem))
		{
			return false;
		}
		Rig.AbilitySystem->ApplyGameplayEffectToSelf(Init.GetDefaultObject(), 1.f, Rig.AbilitySystem->MakeEffectContext());
		Rig.Context = NewObject<UHollowVisorContext>(GetTransientPackage());
		Rig.Context->Initialize(Rig.Controller);
		Rig.Objective = Rig.Context->GetObjective();
		return Test.TestNotNull(TEXT("The context's objective"), Rig.Objective);
	}

	template <typename TActor>
	TActor* SpawnPlaying(UWorld* World, const TCHAR* ClassPath, const FTransform& Transform)
	{
		const TSubclassOf<TActor> Class = LoadClass<TActor>(nullptr, ClassPath);
		TActor* Actor = Class ? World->SpawnActor<TActor>(Class, Transform) : nullptr;
		if (Actor && !Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		return Actor;
	}

	/** The real W_Visor's waypoint layer (it carries W_VisorWaypoint as its marker), bound to Context, ViewSize big. */
	UHollowWaypointLayerWidget* MakeLayer(FAutomationTestBase& Test, UWorld* World, UHollowVisorContext* Context)
	{
		const TSubclassOf<UUserWidget> VisorClass = LoadClass<UUserWidget>(nullptr, VisorPath);
		UUserWidget* Visor = VisorClass ? CreateWidget<UUserWidget>(World, VisorClass) : nullptr;
		UHollowWaypointLayerWidget* Layer = Visor && Visor->WidgetTree ? Cast<UHollowWaypointLayerWidget>(Visor->WidgetTree->FindWidget(TEXT("Waypoints"))) : nullptr;
		if (!Test.TestNotNull(TEXT("W_Visor's waypoint layer"), Layer))
		{
			return nullptr;
		}
		Layer->SetViewSizeForTests(ViewSize);
		Layer->SetContext(Context);
		return Layer;
	}

	const UHollowWaypointLayerWidget::FShown* Find(const UHollowWaypointLayerWidget& Layer, const EHollowWaypointKind Kind)
	{
		return Layer.GetShown().FindByPredicate([Kind](const UHollowWaypointLayerWidget::FShown& Shown) { return Shown.Kind == Kind; });
	}

	void SetKind(AHollowHubTerminal* Terminal, const EHollowTerminalKind Kind)
	{
		// The kind is level data (EditAnywhere): set it the way the level does.
		const FEnumProperty* Property = CastField<FEnumProperty>(AHollowHubTerminal::StaticClass()->FindPropertyByName(TEXT("Kind")));
		if (Property && Terminal)
		{
			Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Terminal), static_cast<int64>(Kind));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowObjectiveRulesTest, "Hollow.UI.Objective.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowObjectiveRulesTest::RunTest(const FString& Parameters)
{
	// UX-01 (ADR-0042): the objective is a pure function of what this machine knows.
	using namespace HollowObjectiveTests;
	using S = EHollowObjectiveStep;
	using K = EHollowWaypointKind;
	FHollowObjectiveFacts Facts;
	TestEqual(TEXT("Neither hub nor raid: nothing to say"), HollowObjectiveRules::Pick(Facts).Step, S::None);
	TestTrue(TEXT("...and no text"), HollowObjectiveRules::Pick(Facts).Text.IsEmpty());

	// Hub: the workbench or locker first, then the contracts console (the host launches, a guest waits for it).
	Facts.bInHub = true;
	Facts.bHost = true;
	const FHollowObjective Prepare = HollowObjectiveRules::Pick(Facts);
	TestEqual(TEXT("Hub: prepare first"), Prepare.Step, S::HubPrepare);
	TestTrue(TEXT("...the three terminals marked"), Prepare.Waypoints == TArray<K>({K::Contracts, K::Workbench, K::Locker}));
	Facts.bPrepared = true;
	TestEqual(TEXT("Prepared, host: launch"), HollowObjectiveRules::PickStep(Facts), S::HubLaunch);
	Facts.bHost = false;
	TestEqual(TEXT("Prepared, guest: the host launches"), HollowObjectiveRules::PickStep(Facts), S::HubWaitHost);
	TestFalse(TEXT("...the lines differ"), HollowObjectiveRules::StepText(S::HubLaunch, Facts).EqualTo(HollowObjectiveRules::StepText(S::HubWaitHost, Facts)));

	// Raid: deploy, find the core, purify or carry it, guard the ritual, deliver, hold, the end.
	Facts = FHollowObjectiveFacts();
	Facts.bInRaid = true;
	Facts.bInHub = true;   // a raid wins (a hub map without a raid id is the hub)
	Facts.Phase = EHollowRaidPhase::Deploying;
	TestEqual(TEXT("Deploying"), HollowObjectiveRules::PickStep(Facts), S::Deploying);
	TestTrue(TEXT("...nothing marked yet"), HollowObjectiveRules::Pick(Facts).Waypoints.IsEmpty());
	Facts.Phase = EHollowRaidPhase::InRaid;
	const FHollowObjective Find = HollowObjectiveRules::Pick(Facts);
	TestTrue(TEXT("In the raid: find the core, the vault and the extraction marked"), Find.Step == S::FindCore && Find.Waypoints == TArray<K>({K::Vault, K::Extraction}));
	Facts.bCoreLifted = true;
	TestEqual(TEXT("Lifted, no altar (the Docks): deliver"), HollowObjectiveRules::PickStep(Facts), S::DeliverCore);
	TestTrue(TEXT("...only the extraction marked"), HollowObjectiveRules::Pick(Facts).Waypoints == TArray<K>({K::Extraction}));
	Facts.bPurifiableCore = true;
	TestEqual(TEXT("A purifiable core but no free altar: deliver"), HollowObjectiveRules::PickStep(Facts), S::DeliverCore);
	Facts.bAltarIdle = true;
	Facts.CoreCredits = CoreCredits;
	Facts.PurifiedCredits = PurifiedCredits;
	const FHollowObjective Choice = HollowObjectiveRules::Pick(Facts);
	TestTrue(TEXT("The Cathedral: purify or carry it, altar and extraction marked"), Choice.Step == S::PurifyOrExtract
		&& Choice.Waypoints == TArray<K>({K::Altar, K::Extraction}));
	TestTrue(FString::Printf(TEXT("...with both prices (%s)"), *Choice.Text.ToString()), Choice.Text.ToString().Contains(HollowObjectiveRules::Credits(PurifiedCredits).ToString())
		&& Choice.Text.ToString().Contains(HollowObjectiveRules::Credits(CoreCredits).ToString()));
	TestEqual(TEXT("Credits read like the hub's"), HollowObjectiveRules::Credits(PurifiedCredits).ToString(), FText::Format(INVTEXT("{0} cr"), FText::AsNumber(PurifiedCredits)).ToString());
	Facts.bRitual = true;
	TestEqual(TEXT("A core on the altar: guard the ritual"), HollowObjectiveRules::PickStep(Facts), S::GuardRitual);
	Facts.bRitual = false;
	Facts.Phase = EHollowRaidPhase::Extraction;
	Facts.SecondsLeft = DepartureSeconds;
	const FHollowObjective Hold = HollowObjectiveRules::Pick(Facts);
	TestTrue(FString::Printf(TEXT("The shuttle counts down: hold the zone (%s)"), *Hold.Text.ToString()), Hold.Step == S::HoldZone
		&& Hold.Text.ToString().Contains(HollowObjectiveRules::Clock(DepartureSeconds).ToString()) && HollowObjectiveRules::IsTimed(Hold.Step));
	TestEqual(TEXT("The clock reads m:ss"), HollowObjectiveRules::Clock(DepartureSeconds).ToString(), FString(TEXT("0:42")));
	Facts.bAlive = false;
	TestEqual(TEXT("Dead: down, the squad can still extract"), HollowObjectiveRules::PickStep(Facts), S::Down);
	TestTrue(TEXT("...nothing marked"), HollowObjectiveRules::Pick(Facts).Waypoints.IsEmpty());
	Facts.Phase = EHollowRaidPhase::Debrief;
	TestEqual(TEXT("Debrief: the raid is over, dead or alive"), HollowObjectiveRules::PickStep(Facts), S::RaidOver);
	for (const S Step : {S::HubPrepare, S::HubLaunch, S::HubWaitHost, S::Deploying, S::FindCore, S::PurifyOrExtract, S::GuardRitual, S::DeliverCore, S::HoldZone, S::Down, S::RaidOver})
	{
		TestFalse(FString::Printf(TEXT("Step %d has a line"), static_cast<int32>(Step)), HollowObjectiveRules::StepText(Step, Facts).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWaypointMathTest, "Hollow.UI.Waypoint.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWaypointMathTest::RunTest(const FString& Parameters)
{
	// UX-01: a place in view sits on its projection; one outside the view sits on the inset edge with an arrow toward it.
	using namespace HollowObjectiveTests;
	const FVector Eye = FVector::ZeroVector;
	const FRotator Forward = FRotator::ZeroRotator;   // looking along +X, Y right, Z up
	FVector2D At;
	TestTrue(TEXT("Straight ahead projects to the centre"), HollowWaypointMath::Project(Eye, Forward, FovDegrees, ViewSize, FVector(1000.0, 0.0, 0.0), At)
		&& At.Equals(ViewSize * 0.5, Tolerance));
	TestTrue(TEXT("45 deg right with a 90 deg view: the right edge"), HollowWaypointMath::Project(Eye, Forward, FovDegrees, ViewSize, FVector(1000.0, 1000.0, 0.0), At)
		&& FMath::IsNearlyEqual(At.X, ViewSize.X, Tolerance) && FMath::IsNearlyEqual(At.Y, ViewSize.Y * 0.5, Tolerance));
	TestTrue(TEXT("Above: higher on screen"), HollowWaypointMath::Project(Eye, Forward, FovDegrees, ViewSize, FVector(1000.0, 0.0, 200.0), At) && At.Y < ViewSize.Y * 0.5);
	TestFalse(TEXT("Behind: no projection"), HollowWaypointMath::Project(Eye, Forward, FovDegrees, ViewSize, FVector(-1000.0, 0.0, 0.0), At));
	TestTrue(TEXT("Behind to the right: the direction still points right"), HollowWaypointMath::ScreenDirection(Eye, Forward, FVector(-1000.0, 500.0, 0.0)).X > 0.0);
	TestTrue(TEXT("Straight behind: down, steady"), HollowWaypointMath::ScreenDirection(Eye, Forward, FVector(-1000.0, 0.0, 0.0)).Equals(FVector2D(0.0, 1.0)));
	TestTrue(TEXT("A turned view turns the direction (looking right, a place ahead in X is to the left)"),
		HollowWaypointMath::ScreenDirection(Eye, FRotator(0.0, 90.0, 0.0), FVector(1000.0, 0.0, 0.0)).X < 0.0);

	const FHollowWaypointPlacement InView = HollowWaypointMath::Place(ViewSize, Margin, true, FVector2D(800.0, 400.0), FVector2D::ZeroVector);
	TestTrue(TEXT("In view: on the place, no arrow"), InView.bOnScreen && InView.Position.Equals(FVector2D(800.0, 400.0)));
	const FHollowWaypointPlacement Right = HollowWaypointMath::Place(ViewSize, Margin, false, FVector2D::ZeroVector, FVector2D(1.0, 0.0));
	TestTrue(TEXT("Off to the right: the right inset edge, arrow at 90 deg"), !Right.bOnScreen && FMath::IsNearlyEqual(Right.Position.X, ViewSize.X - Margin, Tolerance)
		&& FMath::IsNearlyEqual(Right.Position.Y, ViewSize.Y * 0.5, Tolerance) && FMath::IsNearlyEqual(Right.ArrowDegrees, 90.f, 0.01f));
	const FHollowWaypointPlacement Down = HollowWaypointMath::Place(ViewSize, Margin, false, FVector2D::ZeroVector, FVector2D(0.0, 1.0));
	TestTrue(TEXT("Behind: the bottom edge, arrow down"), FMath::IsNearlyEqual(Down.Position.Y, ViewSize.Y - Margin, Tolerance) && FMath::IsNearlyEqual(FMath::Abs(Down.ArrowDegrees), 180.f, 0.01f));
	const FHollowWaypointPlacement Up = HollowWaypointMath::Place(ViewSize, Margin, false, FVector2D::ZeroVector, FVector2D(0.0, -1.0));
	TestTrue(TEXT("Up: the top edge, arrow up"), FMath::IsNearlyEqual(Up.Position.Y, Margin, Tolerance) && FMath::IsNearlyEqual(Up.ArrowDegrees, 0.f, 0.01f));
	const FHollowWaypointPlacement Corner = HollowWaypointMath::Place(ViewSize, Margin, true, FVector2D(ViewSize.X * 3.0, ViewSize.Y * 0.5 - 100.0), FVector2D::ZeroVector);
	TestTrue(TEXT("Projected far outside: clamped onto the edge along its own direction"), !Corner.bOnScreen
		&& FMath::IsNearlyEqual(Corner.Position.X, ViewSize.X - Margin, Tolerance) && Corner.Position.Y < ViewSize.Y * 0.5);
	const FHollowWaypointPlacement Inside = HollowWaypointMath::Place(ViewSize, Margin, true, FVector2D(ViewSize.X - Margin * 0.5, ViewSize.Y * 0.5), FVector2D::ZeroVector);
	TestFalse(TEXT("In view but inside the margin: an edge marker"), Inside.bOnScreen);
	TestEqual(TEXT("Metres are whole"), HollowWaypointMath::Metres(FVector::ZeroVector, FVector(4249.0, 0.0, 0.0)), 42);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowVisorObjectiveTest, "Hollow.UI.Visor.Objective",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowVisorObjectiveTest::RunTest(const FString& Parameters)
{
	// UX-01 (ADR-0042) with the real core, altar, extraction zone and W_Visor: the objective follows the raid's events;
	// the waypoints mark the vault (the resting core), the altar and the extraction, in view or at the edge; the
	// prices are this player's own (the core bonus); the countdown redraws on a timer. No Tick anywhere.
	using namespace HollowObjectiveTests;
	using S = EHollowObjectiveStep;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	UWorld* World = Rig.World.GetTestWorld();
	TestEqual(TEXT("No raid, no hub: nothing to say"), Rig.Objective->GetObjective().Step, S::None);
	AHollowExtractionZone* Zone = SpawnPlaying<AHollowExtractionZone>(World, ZoneClass, FTransform(FVector(ZoneBehind, 0.0, 0.0)));
	AHollowAetherCore* Core = SpawnPlaying<AHollowAetherCore>(World, CoreClass, FTransform(FVector(CoreAhead, 0.0, 0.0)));
	UHollowWaypointLayerWidget* Layer = MakeLayer(*this, World, Rig.Context);
	const UHollowVisorStyle* Style = Rig.Context->GetStyle();
	if (!TestTrue(TEXT("The zone, the core, the layer, the style"), Zone && Core && Layer && Style))
	{
		return false;
	}
	for (const EHollowWaypointKind Kind : TEnumRange<EHollowWaypointKind>())
	{
		TestNotNull(TEXT("DA_VisorStyle marks every kind of place"), Style->GetWaypointLook(Kind));
	}
	TestEqual(TEXT("...one look each"), Style->Waypoints.Num(), WaypointKinds);

	UHollowRaidStateComponent* Raid = Rig.GameState->GetRaidState();
	FHollowRaidNetState Net;
	Net.RaidId = FGuid::NewGuid();
	Net.Phase = EHollowRaidPhase::Deploying;
	Raid->SetState(Net);
	TestEqual(TEXT("Deploying"), Rig.Objective->GetObjective().Step, S::Deploying);
	TestFalse(TEXT("...the layer idles: nothing to mark"), Layer->IsUpdating());
	Net.Phase = EHollowRaidPhase::InRaid;
	Raid->SetState(Net);
	TestEqual(TEXT("In the raid: find the core"), Rig.Objective->GetObjective().Step, S::FindCore);
	const UHollowWaypointLayerWidget::FShown* Vault = Find(*Layer, EHollowWaypointKind::Vault);
	const UHollowWaypointLayerWidget::FShown* Evac = Find(*Layer, EHollowWaypointKind::Extraction);
	TestTrue(TEXT("...the layer follows the view on its timer"), Layer->IsUpdating());
	TestTrue(TEXT("The vault (the resting core) in view, 6 m ahead"), Vault && Vault->Placement.bOnScreen && Vault->Metres == FMath::RoundToInt32(CoreAhead / 100.0));
	TestTrue(TEXT("The extraction behind: at the bottom edge, arrow down"), Evac && !Evac->Placement.bOnScreen && FMath::IsNearlyEqual(FMath::Abs(Evac->Placement.ArrowDegrees), 180.f, 1.f));

	// Lifted, no altar (the Docks): deliver it.
	Core->Interact(Rig.Character);
	TestTrue(TEXT("Carried"), Core->GetCarryable()->GetCarryState().IsCarriedBy(Rig.Character));
	TestEqual(TEXT("Lifted: deliver the core"), Rig.Objective->GetObjective().Step, S::DeliverCore);
	Layer->UpdateMarkers();
	TestTrue(TEXT("...only the extraction marked"), Layer->GetShown().Num() == 1 && Find(*Layer, EHollowWaypointKind::Extraction));

	// The Cathedral's altar: purify it or carry it out, at this player's prices.
	AHollowRitualAltar* Altar = SpawnPlaying<AHollowRitualAltar>(World, AltarClass, FTransform(FRotator(0.0, 180.0, 0.0), FVector(AltarAhead, 0.0, 0.0)));
	if (!TestTrue(TEXT("BP_RitualAltar and its definition"), Altar && Altar->GetDefinition()))
	{
		return false;
	}
	const FHollowObjective Choice = Rig.Objective->GetObjective();
	TestEqual(TEXT("An altar waits: purify or carry"), Choice.Step, S::PurifyOrExtract);
	TestTrue(FString::Printf(TEXT("...at the plain prices without a profile (%s)"), *Choice.Text.ToString()),
		Choice.Text.ToString().Contains(HollowObjectiveRules::Credits(PurifiedCredits).ToString()) && Choice.Text.ToString().Contains(HollowObjectiveRules::Credits(CoreCredits).ToString()));
	FMemoryProfile Profile;
	Profile.State.Profile.Upgrades = {CoreBonus};
	Rig.Objective->SetProfileSourceForTests(&Profile);
	const FText Bonus = Rig.Objective->GetObjective().Text;
	TestTrue(FString::Printf(TEXT("...with the ship's core bonus, its own prices (%s)"), *Bonus.ToString()),
		Bonus.ToString().Contains(HollowObjectiveRules::Credits(BonusPurifiedCredits).ToString()) && Bonus.ToString().Contains(HollowObjectiveRules::Credits(BonusCoreCredits).ToString()));
	Layer->UpdateMarkers();
	TestTrue(TEXT("...the altar and the extraction marked"), Find(*Layer, EHollowWaypointKind::Altar) && Find(*Layer, EHollowWaypointKind::Extraction));

	// The ritual, then the purified core: deliver it.
	Altar->Interact(Rig.Character);
	TestEqual(TEXT("On the altar: guard the ritual"), Rig.Objective->GetObjective().Step, S::GuardRitual);
	Advance(Rig.World, Altar->GetDefinition()->RitualSeconds + Altar->GetDefinition()->PulseSeconds);
	TestTrue(TEXT("Purified and free"), Altar->GetAltarState().Phase == EHollowAltarPhase::Spent && !Core->GetCarryable()->IsHeld());
	TestEqual(TEXT("A purified core: deliver it (one ritual a raid)"), Rig.Objective->GetObjective().Step, S::DeliverCore);

	// The shuttle counts down; the tracker redraws once a second.
	Net.Phase = EHollowRaidPhase::Extraction;
	Net.PhaseEndServerTime = static_cast<float>(Raid->GetServerTime()) + DepartureSeconds;
	Raid->SetState(Net);
	const FText Holding = Rig.Objective->GetObjective().Text;
	TestTrue(FString::Printf(TEXT("Hold the zone (%s)"), *Holding.ToString()), Rig.Objective->GetObjective().Step == S::HoldZone
		&& Holding.ToString().Contains(HollowObjectiveRules::Clock(DepartureSeconds).ToString()));
	Advance(Rig.World, 1.5f);
	TestTrue(FString::Printf(TEXT("...a second later (%s)"), *Rig.Objective->GetObjective().Text.ToString()),
		Rig.Objective->GetObjective().Text.ToString().Contains(HollowObjectiveRules::Clock(DepartureSeconds - 1).ToString()));

	// Down, then over.
	Rig.AbilitySystem->AddLooseGameplayTag(HollowTags::State_Dead);
	TestEqual(TEXT("Dead: down"), Rig.Objective->GetObjective().Step, S::Down);
	TestFalse(TEXT("...nothing marked"), Layer->IsUpdating());
	Rig.AbilitySystem->RemoveLooseGameplayTag(HollowTags::State_Dead);
	Net.Phase = EHollowRaidPhase::Debrief;
	Raid->SetState(Net);
	TestEqual(TEXT("Debrief: over"), Rig.Objective->GetObjective().Step, S::RaidOver);

	Rig.Objective->SetProfileSourceForTests(nullptr);
	Layer->SetContext(nullptr);
	Rig.Context->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowVisorHubObjectiveTest, "Hollow.UI.Visor.HubObjective",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowVisorHubObjectiveTest::RunTest(const FString& Parameters)
{
	// UX-01: in the hub the three terminals are marked; the workbench or locker first, then the contracts console;
	// under an open terminal's screen the waypoints step aside.
	using namespace HollowObjectiveTests;
	using S = EHollowObjectiveStep;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	UWorld* World = Rig.World.GetTestWorld();
	AHollowHubTerminal* Contracts = World->SpawnActor<AHollowHubTerminal>(FVector(TerminalAhead, 0.0, 0.0), FRotator(0.0, 180.0, 0.0));
	AHollowHubTerminal* Workbench = World->SpawnActor<AHollowHubTerminal>(FVector(TerminalAhead, TerminalSide, 0.0), FRotator(0.0, 180.0, 0.0));
	AHollowHubTerminal* Locker = World->SpawnActor<AHollowHubTerminal>(FVector(TerminalAhead, -TerminalSide, 0.0), FRotator(0.0, 180.0, 0.0));
	SetKind(Contracts, EHollowTerminalKind::Contracts);
	SetKind(Workbench, EHollowTerminalKind::Workbench);
	SetKind(Locker, EHollowTerminalKind::Locker);
	UHollowHubTerminalSubsystem* Terminals = UHollowHubTerminalSubsystem::Get(World);
	UHollowWaypointLayerWidget* Layer = MakeLayer(*this, World, Rig.Context);
	if (!TestTrue(TEXT("Terminals, their subsystem, the layer"), Contracts && Workbench && Locker && Terminals && Layer))
	{
		return false;
	}
	Rig.Objective->Refresh();
	TestEqual(TEXT("The hub: prepare first"), Rig.Objective->GetObjective().Step, S::HubPrepare);
	Layer->UpdateMarkers();
	TestEqual(TEXT("...the three terminals marked"), Layer->GetShown().Num(), 3);
	const UHollowWaypointLayerWidget::FShown* Workshop = Find(*Layer, EHollowWaypointKind::Workbench);
	const UHollowWaypointLayerWidget::FShown* Store = Find(*Layer, EHollowWaypointKind::Locker);
	TestTrue(TEXT("...the workbench to the right, the locker to the left"), Workshop && Store && Workshop->Placement.Position.X > ViewSize.X * 0.5
		&& Store->Placement.Position.X < ViewSize.X * 0.5);

	Terminals->Open(Rig.Controller, Workbench);
	TestTrue(TEXT("A screen open: the waypoints step aside"), Rig.Objective->IsScreenOpen() && !Layer->IsUpdating() && Layer->GetShown().IsEmpty());
	Terminals->Close(Rig.Controller);
	TestEqual(TEXT("Closed: the host goes to the contracts console"), Rig.Objective->GetObjective().Step, S::HubLaunch);
	TestTrue(TEXT("...the markers are back"), Layer->IsUpdating() && Layer->GetShown().Num() == 3);
	Layer->SetContext(nullptr);
	Rig.Context->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowVisorControlsHintTest, "Hollow.UI.Visor.ControlsHint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowVisorControlsHintTest::RunTest(const FString& Parameters)
{
	// UX-01: the cheat sheet's lines line up (monospaced), each class reads its own labels, H / View folds it and the
	// choice is the game's setting. The keys themselves come from the local player's mappings (checked in game).
	using namespace HollowObjectiveTests;
	const FText Lines = HollowControlsHintRules::Lines({{INVTEXT("LMB"), INVTEXT("Plasma cutter")}, {INVTEXT("RT"), INVTEXT("Welder")}, {INVTEXT("Space"), INVTEXT("Jump")}});
	TestEqual(TEXT("Keys padded to one column"), Lines.ToString(), FString(TEXT("LMB    Plasma cutter\nRT     Welder\nSpace  Jump")));

	const UHollowControlsHintSet* Rows = GetDefault<UHollowUISettings>()->ControlsHint.LoadSynchronous();
	const UHollowClassDefinition* EngineerClass = UHollowClassDefinition::Load(Engineer);
	const UHollowClassDefinition* OccultistClass = UHollowClassDefinition::Load(Occultist);
	if (!TestNotNull(TEXT("DA_ControlsHint"), Rows) || !TestTrue(TEXT("Both classes"), EngineerClass && OccultistClass))
	{
		return false;
	}
	TestEqual(TEXT("Nine rows"), Rows->Rows.Num(), ControlRows);
	const FHollowControlsHintRow* Primary = Rows->Rows.FindByPredicate([](const FHollowControlsHintRow& Row) { return Row.Action.GetAssetName() == TEXT("IA_PrimaryUse"); });
	const FHollowControlsHintRow* Secondary = Rows->Rows.FindByPredicate([](const FHollowControlsHintRow& Row) { return Row.Action.GetAssetName() == TEXT("IA_SecondaryUse"); });
	if (!TestTrue(TEXT("The tool rows"), Primary && Secondary))
	{
		return false;
	}
	TestTrue(TEXT("The Engineer cuts, the Occultist shines"), UHollowControlsHintSet::LabelFor(*Primary, EngineerClass->ClassTag).ToString() == TEXT("Plasma cutter")
		&& UHollowControlsHintSet::LabelFor(*Primary, OccultistClass->ClassTag).ToString() == TEXT("Aether lantern"));
	TestTrue(TEXT("...no class: the row's own label"), UHollowControlsHintSet::LabelFor(*Primary, FGameplayTag()).EqualTo(Primary->Label));
	TestTrue(TEXT("The Occultist has no welder: no row"), UHollowControlsHintSet::LabelFor(*Secondary, OccultistClass->ClassTag).IsEmpty()
		&& !UHollowControlsHintSet::LabelFor(*Secondary, EngineerClass->ClassTag).IsEmpty());
	TestTrue(TEXT("Tools are for raids"), Primary->bRaidOnly && Secondary->bRaidOnly);

	FRig Rig;
	UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	const TSubclassOf<UHollowControlsHintWidget> WidgetClass = LoadClass<UHollowControlsHintWidget>(nullptr, ControlsClass);
	if (!TestNotNull(TEXT("The game's settings class"), Settings) || !TestNotNull(TEXT("W_ControlsHint"), WidgetClass.Get()) || !Build(*this, Rig))
	{
		return false;
	}
	UHollowControlsHintWidget* Hint = CreateWidget<UHollowControlsHintWidget>(Rig.World.GetTestWorld(), WidgetClass);
	Hint->SetContext(Rig.Context);
	const bool bShownBefore = Settings->IsControlsHintShown();
	Settings->SetControlsHintShown(true, /*bSave=*/false);
	TestTrue(TEXT("Unfolded, titled without a key here (no local player)"), Hint->IsUnfolded() && Hint->GetTitleText().EqualTo(Rig.Context->GetStyle()->ControlsTitle));
	Settings->SetControlsHintShown(false, /*bSave=*/false);
	TestTrue(TEXT("Folded: the title only"), !Hint->IsUnfolded() && Hint->GetListText().IsEmpty() && Hint->Describe().Contains(TEXT("folded")));
	Settings->SetControlsHintShown(bShownBefore, /*bSave=*/false);
	Hint->SetContext(nullptr);
	Rig.Context->Deinitialize();
	return true;
}

#endif
