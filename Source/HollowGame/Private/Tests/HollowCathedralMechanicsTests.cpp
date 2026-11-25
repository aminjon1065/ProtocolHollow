#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Algo/Count.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Attributes/HollowHealthSet.h"
#include "Audio/HollowSoundBank.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "FX/HollowFx.h"
#include "Gadgets/HollowPulseMineDefinition.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Settings/HollowGameUserSettings.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/ScopeExit.h"
#include "Sound/SoundWave.h"
#include "Tests/HollowDocksTestRooms.h"
#include "Data/HollowBiomeDefinition.h"
#include "Damage/HollowDamageExecution.h"
#include "Damage/HollowDamageSettings.h"
#include "Extraction/HollowExtractionResolver.h"
#include "Extraction/HollowRaidTypes.h"
#include "GameplayEffect.h"
#include "Interaction/HollowInteractionComponent.h"
#include "Interaction/HollowInteractionRules.h"
#include "Interaction/HollowInteractionSettings.h"
#include "Items/HollowItemDefinition.h"
#include "Items/HollowItemTypes.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Tags/HollowGameplayTags.h"
#include "Threat/HollowActivityHeatmapSubsystem.h"
#include "World/HollowArcHazard.h"
#include "World/HollowArcHazardDefinition.h"
#include "World/HollowArcRules.h"
#include "World/HollowRitualAltar.h"
#include "World/HollowRitualAltarDefinition.h"
#include "World/HollowSwitch.h"

namespace HollowCathedralTests
{
	// Content under test (create_cathedral_mechanics.py, create_carry_assets.py, create_gas_assets.py).
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const AltarClass = TEXT("/HollowSector_Cathedral/Mechanics/BP_RitualAltar.BP_RitualAltar_C");
	const TCHAR* const ArcClass = TEXT("/HollowSector_Cathedral/Mechanics/BP_ArcHazard.BP_ArcHazard_C");
	const TCHAR* const BreakerClass = TEXT("/HollowSector_Cathedral/Mechanics/BP_ArcBreaker.BP_ArcBreaker_C");
	const TCHAR* const AltarData = TEXT("/HollowSector_Cathedral/Mechanics/DA_RitualAltar.DA_RitualAltar");
	const TCHAR* const ArcData = TEXT("/HollowSector_Cathedral/Mechanics/DA_ArcHazard.DA_ArcHazard");
	const TCHAR* const BiomeData = TEXT("/HollowSector_Cathedral/Data/DA_Biome_Cathedral.DA_Biome_Cathedral");
	const TCHAR* const CoreItem = TEXT("/Game/Hollow/Items/AetherCore/DA_Item_AetherCore.DA_Item_AetherCore");
	const TCHAR* const PurifiedItem = TEXT("/Game/Hollow/Items/AetherCore/DA_Item_PurifiedCore.DA_Item_PurifiedCore");
	const TCHAR* const CoreDefinition = TEXT("/Game/Hollow/Items/AetherCore/DA_Carryable_AetherCore.DA_Carryable_AetherCore");
	const TCHAR* const NoiseConfig = TEXT("/Game/Hollow/Core/DA_NoiseConfig.DA_NoiseConfig");
	const TCHAR* const MineData = TEXT("/Game/Hollow/Gadgets/PulseMine/DA_PulseMine.DA_PulseMine");
	const FPrimaryAssetId PurifiedId(TEXT("HollowItem"), TEXT("DA_Item_PurifiedCore"));
	// The Cathedral's look (S7-03, create_cathedral_look.py).
	const TCHAR* const LookRoot = TEXT("/HollowSector_Cathedral/Look");
	const TCHAR* const SurfaceMaster = TEXT("/Game/Hollow/World/Materials/M_Hollow_Surface.M_Hollow_Surface");
	const TCHAR* const EmissiveMaster = TEXT("/Game/Hollow/World/Materials/M_Hollow_Emissive.M_Hollow_Emissive");
	const TCHAR* const ReactorGlow = TEXT("/HollowSector_Cathedral/Look/MI_Reactor_Glow.MI_Reactor_Glow");
	const TCHAR* const GridMaterial = TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial");
	const FName FollowResonance(TEXT("FollowResonance"));
	const FName GlassViolet(TEXT("GlassViolet"));
	const FName GlassGold(TEXT("GlassGold"));
	const FName ReactorChannels(TEXT("ReactorGlow"));
	const TCHAR* const ProjectRoots[] = {TEXT("/Game"), TEXT("/HollowSector_Docks"), TEXT("/HollowSector_Cathedral")};

	// S7-02 (ADR-0039): the task's numbers.
	constexpr float RitualSeconds = 20.f;
	constexpr float PulseSeconds = 2.f;
	constexpr int32 Tolls = 10;
	constexpr float HeatBurst = 40.f;
	constexpr float PurifiedFactor = 2.5f;
	constexpr int32 CoreCredits = 500;
	constexpr int32 PurifiedCredits = 1250;
	constexpr float ArcTelegraph = 1.5f;
	constexpr float ArcDischarge = 1.f;
	constexpr float ArcIdle = 3.5f;
	constexpr float ArcDamage = 25.f;
	constexpr int32 AltarPerSector = 1;
	constexpr int32 ArcsPerSector = 2;

	/** Test data. */
	constexpr float TickSeconds = 1.f / 30.f;
	constexpr float CoreDistance = 120.f;
	constexpr float AltarDistance = 250.f;
	/** A room of the test topology around everything (cm). */
	constexpr double RoomHalfSize = 2000.0;
	constexpr double RoomHeight = 800.0;
	constexpr float SeatTolerance = 25.f;
	constexpr float Tolerance = 0.01f;
	/** Far enough from the arc's zone to be outside it. */
	constexpr double OutsideDistance = 500.0;
	/** A moment well inside a phase. */
	constexpr double Margin = 0.1;

	struct FRig
	{
		FTestWorldWrapper World;
		AHollowExoFrameCharacter* Character = nullptr;
		UHollowAbilitySystemComponent* AbilitySystem = nullptr;
		AHollowAetherCore* Core = nullptr;
		UHollowCarryableComponent* Carryable = nullptr;
	};

	void Advance(FRig& Rig, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;   // the timer manager ticks once per frame counter
			Rig.World.TickTestWorld(TickSeconds);
		}
	}

	/** A remote player's ExoFrame with its real attributes, playing; with bCore the real BP_AetherCore in front of it. */
	AHollowExoFrameCharacter* SpawnPlayer(FAutomationTestBase& Test, UWorld* World, const FVector& Location, UHollowAbilitySystemComponent*& OutAbilities)
	{
		AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(Test, World, FTransform(Location));
		const TSubclassOf<UGameplayEffect> Init = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
		if (!Character || !Init)
		{
			return nullptr;
		}
		HollowGameTests::PossessAsPlayer(World, Character);
		Character->DispatchBeginPlay();
		OutAbilities = Cast<UHollowAbilitySystemComponent>(Character->GetAbilitySystemComponent());
		if (OutAbilities)
		{
			OutAbilities->ApplyGameplayEffectToSelf(Init.GetDefaultObject(), 1.f, OutAbilities->MakeEffectContext());
		}
		return Character;
	}

	bool Build(FAutomationTestBase& Test, FRig& Rig, const bool bCore)
	{
		UWorld* World = HollowGameTests::CreateWorld(Test, Rig.World);
		Rig.Character = SpawnPlayer(Test, World, FVector::ZeroVector, Rig.AbilitySystem);
		if (!Test.TestNotNull(TEXT("A player with its attributes"), Rig.AbilitySystem))
		{
			return false;
		}
		if (bCore)
		{
			const TSubclassOf<AHollowAetherCore> Core = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
			if (!Test.TestNotNull(TEXT("BP_AetherCore"), Core.Get()))
			{
				return false;
			}
			Rig.Core = World->SpawnActor<AHollowAetherCore>(Core, FTransform(FVector(CoreDistance, 0.0, 0.0)));
			Rig.Core->DispatchBeginPlay();
			Rig.Carryable = Rig.Core->GetCarryable();
		}
		// One room round everything, so the altar's heat has somewhere to go.
		FHollowSectorTopology Topology;
		FHollowTopologyRoom& Room = Topology.Rooms.AddDefaulted_GetRef();
		Room.Bounds = FBox(FVector(-RoomHalfSize, -RoomHalfSize, -RoomHeight), FVector(RoomHalfSize, RoomHalfSize, RoomHeight));
		if (UHollowSectorTopologySubsystem* TopologySubsystem = UHollowSectorTopologySubsystem::Get(World))
		{
			TopologySubsystem->SetTopology(Topology);
		}
		return true;
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

	float Health(const UAbilitySystemComponent* Abilities)
	{
		return Abilities->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarryHeldTest, "Hollow.Game.Carry.Held",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarryHeldTest::RunTest(const FString& Parameters)
{
	// S7-02 (ADR-0039): a fixture can hold a carryable (nobody lifts or joins it) and change what it counts as; both
	// replicate (push), and the core wears the look of the item it counts as.
	using namespace HollowCathedralTests;
	FRig Rig;
	if (!Build(*this, Rig, /*bCore=*/true))
	{
		return false;
	}
	TArray<FLifetimeProperty> Props;
	static_cast<const UObject*>(Rig.Carryable)->GetLifetimeReplicatedProps(Props);
	auto Replicated = [&Props](const TCHAR* Name)
	{
		const FProperty* Property = UHollowCarryableComponent::StaticClass()->FindPropertyByName(Name);
		return Property && Props.ContainsByPredicate([Property](const FLifetimeProperty& Prop) { return Prop.RepIndex == Property->RepIndex && Prop.bIsPushBased; });
	};
	TestTrue(TEXT("Held and the loot item override replicate (push)"), Replicated(TEXT("bHeld")) && Replicated(TEXT("LootItemOverride")));

	const UHollowCarryableDefinition* Definition = Rig.Carryable->GetDefinition();
	TestEqual(TEXT("A plain core counts as its definition's item"), Rig.Carryable->GetLootItem(), Definition->LootItem);
	UMaterialInterface* PlainLook = Rig.Core->GetVisual()->GetMaterial(0);

	Rig.Carryable->SetHeld(true);
	TestFalse(TEXT("Held: nobody lifts it"), Rig.Core->CanInteract(Rig.Character));
	Rig.Carryable->SetHeld(false);
	TestTrue(TEXT("Free again: it can be lifted"), Rig.Core->CanInteract(Rig.Character));

	const FHollowCarryableLook* Look = Definition->FindLook(PurifiedId);
	if (!TestNotNull(TEXT("The purified look"), Look))
	{
		return false;
	}
	Look->Material.LoadSynchronous();   // the game loads it asynchronously; here it is ready, so the swap is immediate
	Rig.Carryable->SetLootItemOverride(PurifiedId);
	TestEqual(TEXT("It counts as the override"), Rig.Carryable->GetLootItem(), PurifiedId);
	Advance(Rig, TickSeconds * 3.f);
	TestTrue(TEXT("...and wears its look"), Rig.Core->GetVisual()->GetMaterial(0) == Look->Material.Get());
	Rig.Carryable->SetLootItemOverride(FPrimaryAssetId());
	TestTrue(TEXT("Without the override: the plain item and look"), Rig.Carryable->GetLootItem() == Definition->LootItem
		&& Rig.Core->GetVisual()->GetMaterial(0) == PlainLook);

	// PlaceAt: set down where asked (carried or not), every carrier lets go.
	Rig.Core->Interact(Rig.Character);
	const FTransform Where(FVector(CoreDistance, CoreDistance, CoreDistance));
	TestTrue(TEXT("Placed out of its carrier's hands"), Rig.Carryable->PlaceAt(Where, Rig.Character) && !Rig.Carryable->GetCarryState().IsCarried()
		&& !Rig.Character->GetCarrierComponent()->IsCarrying());
	TestTrue(TEXT("...where it was asked"), Rig.Core->GetActorLocation().Equals(Where.GetLocation(), Tolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAltarRitualTest, "Hollow.Game.Altar.Ritual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAltarRitualTest::RunTest(const FString& Parameters)
{
	// S7-02 (ADR-0039), the real BP_RitualAltar: only a carried core is taken; it rests on the bowl, held; the bell tolls
	// at once and every 2 s for 20 s, each toll a Hollow.Noise.Ritual by the player who laid it; its room heats by 40 at
	// once; then the core counts as a purified one and is free again, and the altar is spent. A carrier may focus it.
	using namespace HollowCathedralTests;
	FRig Rig;
	if (!Build(*this, Rig, /*bCore=*/true))
	{
		return false;
	}
	UWorld* World = Rig.World.GetTestWorld();
	// FAB-01: the effects this machine plays, in memory as the altar's preload leaves them before a ritual.
	const UHollowRitualAltarDefinition* AltarDefinition = LoadObject<UHollowRitualAltarDefinition>(nullptr, AltarData);
	if (!TestNotNull(TEXT("DA_RitualAltar"), AltarDefinition))
	{
		return false;
	}
	const TSoftObjectPtr<UFXSystemAsset> RitualFx = HollowFx::Chosen(AltarDefinition->RitualFx);
	const TSoftObjectPtr<UFXSystemAsset> FinaleFx = HollowFx::Chosen(AltarDefinition->PurifiedFx);
	RitualFx.LoadSynchronous();
	FinaleFx.LoadSynchronous();
	TArray<const FHollowFxSpec*> Played;   // what this machine starts (headless: Niagara renders nothing)
	const FDelegateHandle FxListen = HollowFx::OnPlay().AddLambda([&Played](const FHollowFxSpec& Spec, const UFXSystemAsset*) { Played.Add(&Spec); });
	ON_SCOPE_EXIT
	{
		HollowFx::OnPlay().Remove(FxListen);
	};
	AHollowRitualAltar* Altar = SpawnPlaying<AHollowRitualAltar>(World, AltarClass, FTransform(FRotator(0.0, 180.0, 0.0), FVector(AltarDistance, 0.0, 0.0)));
	if (!TestNotNull(TEXT("BP_RitualAltar"), Altar) || !TestNotNull(TEXT("its definition"), Altar->GetDefinition()))
	{
		return false;
	}
	TArray<FHollowNoiseReport> Reports;
	UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(World);
	if (!TestNotNull(TEXT("Noise subsystem"), Noise))
	{
		return false;
	}
	Noise->SetConfig(LoadObject<UHollowNoiseConfig>(nullptr, NoiseConfig));
	Noise->OnNoiseReported.AddLambda([&Reports](const FHollowNoiseReport& Report)
	{
		if (Report.Event.NoiseTag == HollowTags::Noise_Ritual)
		{
			Reports.Add(Report);
		}
	});
	UHollowActivityHeatmapSubsystem* Heatmap = UHollowActivityHeatmapSubsystem::Get(World);

	TestFalse(TEXT("Empty-handed: nothing to lay on it"), Altar->CanInteract(Rig.Character));
	Rig.Core->Interact(Rig.Character);
	TestTrue(TEXT("Carrying a core: it takes it"), Altar->CanInteract(Rig.Character)
		&& Altar->GetInteractionInfo(Rig.Character).Prompt.EqualTo(Altar->GetDefinition()->PlacePrompt));
	FHollowInteractionCheck Check = Rig.Character->GetInteractionComponent()->MeasureRequest(Altar);
	TestFalse(TEXT("A carrier may hand it over (a carry receiver, CarryReceiverAllowedTags)"), Check.bInteractorBlocked);
	const UHollowInteractionSettings* Interaction = GetDefault<UHollowInteractionSettings>();
	const EHollowInteractionRejection Rejection = HollowInteractionRules::ValidateRequest(Check, Interaction->InteractionRange + Interaction->ServerRangeTolerance,
		Interaction->MinInteractInterval);
	TestEqual(TEXT("...and the server accepts it: its own core hides nothing, the altar's open middle counts as seen"), Rejection, EHollowInteractionRejection::None);
	TestTrue(TEXT("...but nothing else while carrying"), Rig.Character->GetInteractionComponent()->MeasureRequest(Rig.Core).bInteractorBlocked);
	Rig.Carryable->SetLootItemOverride(PurifiedId);
	TestFalse(TEXT("It takes only what it purifies"), Altar->CanInteract(Rig.Character));
	Rig.Carryable->SetLootItemOverride(FPrimaryAssetId());

	if (!TestNotNull(TEXT("Heatmap"), Heatmap))
	{
		return false;
	}
	const float HeatBefore = Heatmap->GetRoomHeat(0);
	Altar->Interact(Rig.Character);
	TestTrue(TEXT("The ritual runs"), Altar->GetAltarState().Phase == EHollowAltarPhase::Ritual && Altar->GetAltarState().Held == Rig.Core);
	TestTrue(TEXT("The core is out of its carrier's hands"), !Rig.Carryable->GetCarryState().IsCarried() && !Rig.Character->GetCarrierComponent()->IsCarrying());
	TestTrue(TEXT("...on the bowl"), Rig.Core->GetActorLocation().Equals(Altar->GetSeatTransform().GetLocation(), SeatTolerance));
	TestTrue(TEXT("...held: nobody lifts it"), Rig.Carryable->IsHeld() && !Rig.Core->CanInteract(Rig.Character));
	TestFalse(TEXT("No second core while it runs"), Altar->CanInteract(Rig.Character));
	TestEqual(TEXT("The first toll at once"), Reports.Num(), 1);
	TestTrue(TEXT("FAB-01 (ADR-0051): the violet Aether is drawn into the core while it runs"), Played.Contains(&AltarDefinition->RitualFx));
	TestFalse(TEXT("...no finale yet"), Played.Contains(&AltarDefinition->PurifiedFx));
	TestTrue(TEXT("Its room heats at once, by the burst at least (the toll's noise adds its own)"), Heatmap->GetRoomHeat(0) - HeatBefore >= HeatBurst);

	Advance(Rig, RitualSeconds * 0.5f);
	TestTrue(TEXT("Halfway: still held, still the plain core"), Rig.Carryable->IsHeld() && Rig.Carryable->GetLootItem() != PurifiedId);
	Advance(Rig, RitualSeconds * 0.5f + PulseSeconds * 0.5f);
	TestEqual(TEXT("A toll every 2 s for 20 s"), Reports.Num(), Tolls);
	TestTrue(TEXT("Every toll is the layer's noise at the bowl"), !Reports.ContainsByPredicate([&Rig](const FHollowNoiseReport& Report)
		{
			return Report.Event.Instigator.Get() != Rig.Character;
		}));
	TestTrue(TEXT("Done: the core is purified and free"), Rig.Carryable->GetLootItem() == PurifiedId && !Rig.Carryable->IsHeld()
		&& Rig.Core->CanInteract(Rig.Character));
	TestTrue(TEXT("The altar is spent: one ritual a raid"), Altar->GetAltarState().Phase == EHollowAltarPhase::Spent);
	TestNull(TEXT("FAB-01: the ritual's effect is over"), Altar->GetRitualEffect());
	TestEqual(TEXT("...the gold finale played once (nothing under Reduced Flashes)"),
		static_cast<int32>(Algo::Count(Played, &AltarDefinition->PurifiedFx)), FinaleFx.IsNull() ? 0 : 1);
	const UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(World);
	TestTrue(TEXT("Its end is heard once (S7-03)"), Audio && Audio->GetPlayCount(HollowTags::Sound_Altar_Purified) == 1);
	TestTrue(TEXT("...and every toll was a bell"), Audio && Audio->GetPlayCount(HollowTags::Noise_Ritual) == Tolls);
	Rig.Core->Interact(Rig.Character);
	TestFalse(TEXT("...a purified core is not taken again"), Altar->CanInteract(Rig.Character));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFabCoreAuraTest, "Hollow.Game.Fab.CoreAura",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFabCoreAuraTest::RunTest(const FString& Parameters)
{
	// FAB-01 (ADR-0051), the real BP_AetherCore: it wears the data's aura, the cyan pulse, and the gold glow once it counts
	// as a purified core; under Reduced Flashes their dimmer variants. Then the steady glows have a dimmer variant and the
	// flashes (the altar's finale, the mine's blast) none, so they play nothing.
	using namespace HollowCathedralTests;
	const UHollowCarryableDefinition* Core = LoadObject<UHollowCarryableDefinition>(nullptr, CoreDefinition);
	const UHollowRitualAltarDefinition* Altar = LoadObject<UHollowRitualAltarDefinition>(nullptr, AltarData);
	const UHollowPulseMineDefinition* Mine = LoadObject<UHollowPulseMineDefinition>(nullptr, MineData);
	UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	if (!TestNotNull(TEXT("DA_Carryable_AetherCore"), Core) || !TestNotNull(TEXT("DA_RitualAltar"), Altar) || !TestNotNull(TEXT("DA_PulseMine"), Mine)
		|| !TestNotNull(TEXT("The game's settings"), Settings))
	{
		return false;
	}
	const FHollowFxSpec* Plain = Core->FindAura(Core->LootItem);
	const FHollowFxSpec* Purified = Core->FindAura(PurifiedId);
	if (!TestTrue(TEXT("A plain and a purified aura"), Plain && Purified && Plain != Purified))
	{
		return false;
	}
	const auto IsSystem = [](const TSoftObjectPtr<UFXSystemAsset>& System) { return Cast<UNiagaraSystem>(System.LoadSynchronous()) != nullptr; };
	TestTrue(TEXT("Every steady glow has its Niagara system and a dimmer one"), IsSystem(Plain->System) && IsSystem(Plain->ReducedFlashesSystem)
		&& IsSystem(Purified->System) && IsSystem(Purified->ReducedFlashesSystem) && IsSystem(Altar->RitualFx.System) && IsSystem(Altar->RitualFx.ReducedFlashesSystem));
	TestTrue(TEXT("...the flashes have theirs and none to dim"), IsSystem(Altar->PurifiedFx.System) && Altar->PurifiedFx.ReducedFlashesSystem.IsNull()
		&& IsSystem(Mine->BlastFx.System) && Mine->BlastFx.ReducedFlashesSystem.IsNull());
	TestTrue(TEXT("...and a one-shot stops on its own"), Altar->PurifiedFx.Seconds > 0.f && Mine->BlastFx.Seconds > 0.f);
	// Our copies only, never the pack's own systems: no light on every particle, no haze, no GPU simulation (on Mac Low the
	// pack's ritual seen up close cost 31 FPS against 61; its GPU sparks alone about 1 ms however few; ours 0.25 ms,
	// create_fab_fx.py).
	const auto IsLean = [](const TSoftObjectPtr<UFXSystemAsset>& Asset)
	{
		const UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset.LoadSynchronous());
		if (!System || !System->GetPathName().StartsWith(TEXT("/Game/Hollow/FX/Fab/")))
		{
			return Asset.IsNull();   // no system at all is lean
		}
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
			if (!Handle.GetIsEnabled() || !Data)
			{
				continue;
			}
			if (Handle.GetName() == FName(TEXT("NE_Haze")) || Data->SimTarget == ENiagaraSimTarget::GPUComputeSim)
			{
				return false;
			}
			for (const UNiagaraRendererProperties* Renderer : Data->GetRenderers())
			{
				if (Renderer && Renderer->GetIsEnabled() && Renderer->IsA<UNiagaraLightRendererProperties>())
				{
					return false;
				}
			}
		}
		return true;
	};
	TestTrue(TEXT("Every effect plays our lean copy (no per-particle light, no haze, nothing simulated on the GPU)"), IsLean(Plain->System) && IsLean(Plain->ReducedFlashesSystem)
		&& IsLean(Purified->System) && IsLean(Purified->ReducedFlashesSystem) && IsLean(Altar->RitualFx.System) && IsLean(Altar->RitualFx.ReducedFlashesSystem)
		&& IsLean(Altar->PurifiedFx.System) && IsLean(Mine->BlastFx.System));

	const bool bReducedBefore = Settings->IsReducedFlashes();
	Settings->SetReducedFlashes(false, /*bSave=*/false);
	const UFXSystemAsset* Last = nullptr;   // the last effect this machine started (headless: Niagara renders nothing)
	int32 Plays = 0;
	const FDelegateHandle FxListen = HollowFx::OnPlay().AddLambda([&Last, &Plays](const FHollowFxSpec&, const UFXSystemAsset* System)
	{
		Last = System;
		++Plays;
	});
	ON_SCOPE_EXIT
	{
		HollowFx::OnPlay().Remove(FxListen);
		Settings->SetReducedFlashes(bReducedBefore, /*bSave=*/false);
	};
	FRig Rig;
	if (!Build(*this, Rig, /*bCore=*/true))
	{
		return false;
	}
	TestTrue(TEXT("The plain core pulses cyan"), Last == Plain->System.Get());
	Rig.Carryable->SetLootItemOverride(PurifiedId);
	TestTrue(TEXT("...purified: the gold glow"), Last == Purified->System.Get());
	Settings->SetReducedFlashes(true, /*bSave=*/false);
	Rig.Carryable->SetLootItemOverride(FPrimaryAssetId());
	TestTrue(TEXT("Reduced Flashes: the dimmer pulse"), Last == Plain->ReducedFlashesSystem.Get());
	Rig.Carryable->SetLootItemOverride(PurifiedId);
	TestTrue(TEXT("...and the dimmer glow"), Last == Purified->ReducedFlashesSystem.Get());
	UWorld* World = Rig.World.GetTestWorld();
	const int32 Before = Plays;
	HollowFx::PlayAt(Altar->PurifiedFx, World, FTransform::Identity);
	HollowFx::PlayAt(Mine->BlastFx, World, FTransform::Identity);
	TestEqual(TEXT("...no finale, no blast"), Plays, Before);
	Settings->SetReducedFlashes(false, /*bSave=*/false);
	HollowFx::PlayAt(Altar->PurifiedFx, World, FTransform::Identity);
	TestTrue(TEXT("Full flashes: the finale plays"), Plays == Before + 1 && Last == Altar->PurifiedFx.System.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArcRulesTest, "Hollow.Game.Arc.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArcRulesTest::RunTest(const FString& Parameters)
{
	// The cycle on the server's clock: warning, strike, rest, again; nothing without power.
	using namespace HollowCathedralTests;
	FHollowArcTimings Timings;
	Timings.TelegraphSeconds = ArcTelegraph;
	Timings.DischargeSeconds = ArcDischarge;
	Timings.IdleSeconds = ArcIdle;
	auto At = [&Timings](const double Seconds) { return HollowArcRules::MomentAt(Seconds, Timings, /*bPowered=*/true); };
	TestTrue(TEXT("Power on: the warning first"), At(0.0).Phase == EHollowArcPhase::Telegraph && FMath::IsNearlyEqual(At(0.0).SecondsToNext, ArcTelegraph));
	TestTrue(TEXT("Then the strike"), At(ArcTelegraph + Margin).Phase == EHollowArcPhase::Discharge);
	TestTrue(TEXT("Then the rest"), At(ArcTelegraph + ArcDischarge + Margin).Phase == EHollowArcPhase::Idle);
	TestTrue(TEXT("And again"), At(Timings.Period() + Margin).Phase == EHollowArcPhase::Telegraph
		&& At(2.0 * Timings.Period() + ArcTelegraph + Margin).Phase == EHollowArcPhase::Discharge);
	TestTrue(TEXT("The rest ends with the period"), FMath::IsNearlyEqual(At(Timings.Period() - Margin).SecondsToNext, Margin, static_cast<double>(Tolerance)));
	TestTrue(TEXT("Off: nothing"), HollowArcRules::MomentAt(ArcTelegraph + Margin, Timings, /*bPowered=*/false).Phase == EHollowArcPhase::Off);
	FHollowArcTimings Broken = Timings;
	Broken.IdleSeconds = 0.f;
	TestTrue(TEXT("A cycle without a rest is no cycle"), !HollowArcRules::IsValid(Broken) && HollowArcRules::MomentAt(Margin, Broken, true).Phase == EHollowArcPhase::Off);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArcHazardTest, "Hollow.Game.Arc.Hazard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArcHazardTest::RunTest(const FString& Parameters)
{
	// S7-02 (ADR-0039), the real BP_ArcHazard: it spawns its breaker (on), warns 1.5 s, then strikes every living player
	// inside for 25 electric damage once a cycle; a player outside takes nothing. Its breaker cuts the power: no strike
	// for two cycles; on again, the warning comes first.
	using namespace HollowCathedralTests;
	FRig Rig;
	if (!Build(*this, Rig, /*bCore=*/false))
	{
		return false;
	}
	UWorld* World = Rig.World.GetTestWorld();
	AHollowArcHazard* Arc = SpawnPlaying<AHollowArcHazard>(World, ArcClass, FTransform(FVector(OutsideDistance, 0.0, 0.0)));
	if (!TestNotNull(TEXT("BP_ArcHazard"), Arc) || !TestNotNull(TEXT("its definition"), Arc->GetDefinition()))
	{
		return false;
	}
	// The player in the zone (standing on its floor), another one outside it.
	Rig.Character->SetActorLocation(Arc->GetActorLocation() + FVector(0.0, 0.0, Rig.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	UHollowAbilitySystemComponent* OutsideAbilities = nullptr;
	AHollowExoFrameCharacter* Outside = SpawnPlayer(*this, World, FVector(0.0, 0.0, Rig.Character->GetActorLocation().Z), OutsideAbilities);
	if (!TestNotNull(TEXT("A second player"), OutsideAbilities))
	{
		return false;
	}
	TestTrue(TEXT("One inside, one outside"), Arc->Contains(Rig.Character->GetActorLocation()) && !Arc->Contains(Outside->GetActorLocation()));
	AHollowSwitch* Breaker = Arc->GetBreaker();
	TestTrue(TEXT("Its breaker, on: powered, warning first"), Breaker && Breaker->IsOn() && Arc->IsPowered()
		&& Arc->GetMoment().Phase == EHollowArcPhase::Telegraph);
	const float Full = Health(Rig.AbilitySystem);
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(World);
	if (!TestNotNull(TEXT("Audio subsystem"), Audio))
	{
		return false;
	}
	Audio->ResetCounts();

	Advance(Rig, ArcTelegraph - static_cast<float>(Margin));
	TestEqual(TEXT("The warning hurts nobody"), Health(Rig.AbilitySystem), Full);
	Advance(Rig, static_cast<float>(2.0 * Margin));
	TestNearlyEqual(TEXT("The strike: 25 electric damage inside"), Health(Rig.AbilitySystem), Full - ArcDamage, Tolerance);
	TestEqual(TEXT("...none outside"), Health(OutsideAbilities), Full);
	Advance(Rig, ArcDischarge);
	TestNearlyEqual(TEXT("Once a strike"), Health(Rig.AbilitySystem), Full - ArcDamage, Tolerance);
	Advance(Rig, ArcIdle + ArcTelegraph);
	TestNearlyEqual(TEXT("The next cycle strikes again"), Health(Rig.AbilitySystem), Full - 2.f * ArcDamage, Tolerance);
	TestEqual(TEXT("Two strikes"), Arc->GetStrikes(), 2);
	// S7-03: every fresh strike crackles first, then cracks (the spawn's own first warning is not news).
	TestEqual(TEXT("A strike sound for each strike"), Audio->GetPlayCount(HollowTags::Sound_Arc_Discharge), 2);
	TestEqual(TEXT("...and a warning before the second"), Audio->GetPlayCount(HollowTags::Sound_Arc_Telegraph), 1);

	Breaker->Interact(Outside);
	TestTrue(TEXT("Breaker off: no power, nothing"), !Breaker->IsOn() && !Arc->IsPowered() && Arc->GetMoment().Phase == EHollowArcPhase::Off);
	const float AfterTwo = Health(Rig.AbilitySystem);
	Advance(Rig, 2.f * (ArcTelegraph + ArcDischarge + ArcIdle));
	TestEqual(TEXT("Two cycles without a strike"), Health(Rig.AbilitySystem), AfterTwo);

	Breaker->Interact(Outside);
	TestTrue(TEXT("On again: the warning first, no strike yet"), Arc->IsPowered() && Arc->GetMoment().Phase == EHollowArcPhase::Telegraph
		&& Health(Rig.AbilitySystem) == AfterTwo);
	Advance(Rig, ArcTelegraph + static_cast<float>(Margin));
	TestNearlyEqual(TEXT("...then it strikes"), Health(Rig.AbilitySystem), AfterTwo - ArcDamage, Tolerance);

	Arc->Destroy();
	TestFalse(TEXT("The breaker goes with its arc"), IsValid(Breaker));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCathedralDataTest, "Hollow.Game.Cathedral.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCathedralDataTest::RunTest(const FString& Parameters)
{
	// The shipped mechanics (create_cathedral_mechanics.py, create_noise_assets.py, create_audio_assets.py, DefaultGame.ini):
	// a purified core is worth 2.5 cores (1250 instead of 500 at extraction); the altar's ritual is 20 s with a bell
	// every 2 s that the AI hears and the players hear as far; the arcs strike 25 electric damage straight through the
	// armour; every Cathedral sector gets one altar and up to two arcs.
	using namespace HollowCathedralTests;
	const UHollowItemDefinition* Core = LoadObject<UHollowItemDefinition>(nullptr, CoreItem);
	const UHollowItemDefinition* Purified = LoadObject<UHollowItemDefinition>(nullptr, PurifiedItem);
	if (!TestNotNull(TEXT("DA_Item_AetherCore"), Core) || !TestNotNull(TEXT("DA_Item_PurifiedCore"), Purified))
	{
		return false;
	}
	TestTrue(TEXT("Purified: 2.5 cores, a core type, not insurable"), Core->BaseValue == CoreCredits
		&& Purified->BaseValue == FMath::RoundToInt32(CoreCredits * PurifiedFactor) && Purified->ItemType == HollowTags::Item_Type_AetherCore && !Purified->bInsurable);
	const FHollowItemCatalog Catalog = FHollowItemCatalog::Load(TArray<FPrimaryAssetId>{PurifiedId});   // as the raid result does: its items
	FHollowRaidContext Raid;
	FHollowItemStack& Loot = Raid.Loot.AddDefaulted_GetRef();
	Loot.Item = PurifiedId;
	Loot.Count = 1;
	TestEqual(TEXT("Extracted with a purified core: 1250 credits"), FHollowExtractionResolver::Resolve(Raid, EHollowRaidOutcome::Extracted, Catalog).Credits,
		PurifiedCredits);
	const UHollowCarryableDefinition* Carryable = LoadObject<UHollowCarryableDefinition>(nullptr, CoreDefinition);
	TestTrue(TEXT("A purified core has its own look"), Carryable && Carryable->FindLook(PurifiedId) && !Carryable->FindLook(PurifiedId)->Material.IsNull());

	const UHollowRitualAltarDefinition* Altar = LoadObject<UHollowRitualAltarDefinition>(nullptr, AltarData);
	if (TestNotNull(TEXT("DA_RitualAltar"), Altar))
	{
		TestTrue(TEXT("It purifies cores in 20 s, a bell every 2 s, +40 heat"), Altar->AcceptedItem == Core->GetPrimaryAssetId() && Altar->ResultItem == PurifiedId
			&& Altar->RitualSeconds == RitualSeconds && Altar->PulseSeconds == PulseSeconds && Altar->NoiseTag == HollowTags::Noise_Ritual
			&& Altar->HeatBurst == HeatBurst);
	}
	const UHollowNoiseConfig* Noise = LoadObject<UHollowNoiseConfig>(nullptr, NoiseConfig);
	const FHollowNoiseRule* Ritual = Noise ? Noise->FindRule(HollowTags::Noise_Ritual) : nullptr;
	const FHollowNoiseRule* Breach = Noise ? Noise->FindRule(HollowTags::Noise_Structural_Breach) : nullptr;
	TestTrue(TEXT("The bell is a noise the AI hears, loud (as a breach)"), Ritual && Breach && Ritual->Loudness >= Breach->Loudness && Ritual->MaxRange > 0.f);
	TestTrue(TEXT("...and a sound in the bank (Hollow.Game.Audio.Consistency: as far)"), UHollowSoundBank::Get() && UHollowSoundBank::Get()->Find(HollowTags::Noise_Ritual));

	const UHollowArcHazardDefinition* Arc = LoadObject<UHollowArcHazardDefinition>(nullptr, ArcData);
	if (TestNotNull(TEXT("DA_ArcHazard"), Arc))
	{
		TestTrue(TEXT("Warning 1.5 s, strike 1 s, rest 3.5 s, 25 damage"), Arc->TelegraphSeconds == ArcTelegraph && Arc->DischargeSeconds == ArcDischarge
			&& Arc->IdleSeconds == ArcIdle && Arc->Damage == ArcDamage);
		const UHollowDamageEffect* Effect = Arc->DamageEffect ? Cast<UHollowDamageEffect>(Arc->DamageEffect.GetDefaultObject()) : nullptr;
		TestTrue(TEXT("Electric damage (GE_Damage_Electric)"), Effect && Effect->DamageType == HollowTags::Damage_Electric);
		const AHollowSwitch* Breaker = Arc->BreakerClass ? Arc->BreakerClass.GetDefaultObject() : nullptr;
		TestTrue(TEXT("Its breaker is a switch (S1-06), on by default"), Breaker && Breaker->IsOn()
			&& Arc->BreakerClass == LoadClass<AHollowSwitch>(nullptr, BreakerClass));
	}
	const FHollowDamageTypeRule* Electric = GetDefault<UHollowDamageSettings>()->FindRule(HollowTags::Damage_Electric);
	TestTrue(TEXT("Electric damage goes through the armour"), Electric && !Electric->bArmorApplies);

	const UHollowBiomeDefinition* Biome = LoadObject<UHollowBiomeDefinition>(nullptr, BiomeData);
	if (TestNotNull(TEXT("DA_Biome_Cathedral"), Biome))
	{
		TestTrue(TEXT("Every Cathedral sector has an altar and an arc"), Biome->GenRules.RequiredSocketTypes.HasTagExact(HollowTags::Socket_Altar)
			&& Biome->GenRules.RequiredSocketTypes.HasTagExact(HollowTags::Socket_ArcHazard));
		auto Rule = [Biome](const FGameplayTag& Socket, const TCHAR* ClassPath)
		{
			const UClass* Class = LoadClass<AActor>(nullptr, ClassPath);
			return Biome->SocketActors.FindByPredicate([&Socket, Class](const FHollowSocketActorRule& Each) { return Each.SocketType == Socket && Each.ActorClass.Get() == Class; });
		};
		const FHollowSocketActorRule* AltarRule = Rule(HollowTags::Socket_Altar, AltarClass);
		const FHollowSocketActorRule* ArcRule = Rule(HollowTags::Socket_ArcHazard, ArcClass);
		TestTrue(TEXT("One altar, the farthest from Spawn"), AltarRule && AltarRule->MaxPerSector == AltarPerSector && AltarRule->Pick == EHollowSocketPick::FarthestFromSpawn);
		TestTrue(TEXT("Up to two arcs"), ArcRule && ArcRule->MaxPerSector == ArcsPerSector);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCathedralLookTest, "Hollow.Game.Cathedral.Look",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCathedralLookTest::RunTest(const FString& Parameters)
{
	// S7-03 (ADR-0040): the Cathedral wears its palette, nothing heavy: every material of its look is an instance of the
	// project's node-only masters; no greybox grid is left on a room; the halls' upper lancets have glass (violet and
	// gold), the crypt and the crossing a cyan reactor glow that follows MPC_Resonance; the project has no wave files.
	using namespace HollowCathedralTests;
	const UMaterial* Surface = LoadObject<UMaterial>(nullptr, SurfaceMaster);
	const UMaterial* Emissive = LoadObject<UMaterial>(nullptr, EmissiveMaster);
	const UMaterialInterface* Grid = LoadObject<UMaterialInterface>(nullptr, GridMaterial);
	if (!TestNotNull(TEXT("M_Hollow_Surface"), Surface) || !TestNotNull(TEXT("M_Hollow_Emissive"), Emissive) || !TestNotNull(TEXT("WorldGridMaterial"), Grid))
	{
		return false;
	}
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	FARFilter Palette;
	Palette.PackagePaths.Add(FName(LookRoot));
	Palette.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
	TArray<FAssetData> Instances;
	Registry.GetAssets(Palette, Instances);
	TestTrue(TEXT("A palette of instances"), Instances.Num() > 0);
	for (const FAssetData& Asset : Instances)
	{
		const UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset.GetAsset());
		const UMaterial* Base = Instance ? Instance->GetMaterial() : nullptr;
		TestTrue(FString::Printf(TEXT("%s: an instance of M_Hollow_Surface or M_Hollow_Emissive"), *Asset.AssetName.ToString()), Base == Surface || Base == Emissive);
	}
	float Follows = 0.f;
	const UMaterialInstanceConstant* Reactor = LoadObject<UMaterialInstanceConstant>(nullptr, ReactorGlow);
	TestTrue(TEXT("The reactor glow follows MPC_Resonance"), Reactor && Reactor->GetScalarParameterValue(FollowResonance, Follows) && Follows > 0.f);

	for (const UHollowRoomDefinition* Room : HollowDocksTestRooms::SectorRoomDefinitions(HollowDocksTestRooms::CathedralRoot))
	{
		const UWorld* Level = Room->RoomLevel.LoadSynchronous();
		if (!TestNotNull(*FString::Printf(TEXT("%s level"), *Room->GetName()), Level) || !Level->PersistentLevel)
		{
			continue;
		}
		int32 GridLeft = 0;
		TMap<FName, int32> Decor;
		for (const AActor* Actor : Level->PersistentLevel->Actors)
		{
			TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);
			for (const UStaticMeshComponent* Mesh : Meshes)
			{
				for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
				{
					GridLeft += Mesh->GetMaterial(Slot) == Grid ? 1 : 0;
				}
				if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Mesh))
				{
					Decor.Add(Instanced->GetFName(), Instanced->GetInstanceCount());
				}
			}
		}
		TestEqual(FString::Printf(TEXT("%s: no greybox grid left"), *Room->GetName()), GridLeft, 0);
		if (Room->Footprint.Z > 1)
		{
			TestTrue(FString::Printf(TEXT("%s: glass in the upper lancets, violet and gold"), *Room->GetName()),
				Decor.FindRef(GlassViolet) > 0 && Decor.FindRef(GlassGold) > 0);
		}
		if (Room->RoomTags.HasTagExact(HollowTags::Room_Type_CoreVault) || Room->RoomTags.HasTagExact(HollowTags::Room_Type_Junction))
		{
			TestTrue(FString::Printf(TEXT("%s: the reactor glows in the floor"), *Room->GetName()), Decor.FindRef(ReactorChannels) > 0);
		}
	}

	FARFilter Waves;
	for (const TCHAR* Root : ProjectRoots)
	{
		Waves.PackagePaths.Add(FName(Root));
	}
	Waves.bRecursivePaths = true;
	Waves.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
	TArray<FAssetData> WaveAssets;
	Registry.GetAssets(Waves, WaveAssets);
	TestEqual(TEXT("No wave files: every sound is a procedural MetaSound"), WaveAssets.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
