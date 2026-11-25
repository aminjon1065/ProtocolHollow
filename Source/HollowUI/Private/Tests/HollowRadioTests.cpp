#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Audio/HollowAudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Foundation/HollowUISettings.h"
#include "Framework/HollowGameState.h"
#include "GameplayTagsManager.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "Radio/HollowRadioRules.h"
#include "Radio/HollowRadioScript.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/HollowLocalizationTestUtils.h"
#include "UObject/Package.h"
#include "ViewModels/HollowRadioModel.h"
#include "ViewModels/HollowVisorContext.h"
#include "Lore/HollowLoreEntryDefinition.h"
#include "Raid/HollowRaidStateComponent.h"
#include "TimerManager.h"
#include "Visor/HollowLoreNoticeWidget.h"
#include "Visor/HollowRadioSubtitleWidget.h"
#include "Visor/HollowVisorStyle.h"
#include "Visor/HollowVisorElement.h"

namespace HollowRadioTests
{
	// Content under test (create_story_assets.py, create_visor_assets.py).
	const TCHAR* const VisorPath = TEXT("/Game/Hollow/UI/Visor/W_Visor.W_Visor_C");
	const FName Docks(TEXT("HollowSector_Docks"));
	const FName Cathedral(TEXT("HollowSector_Cathedral"));
	const FName Briefing(TEXT("Hub.Briefing"));

	/** Test data: a pace easy to count (10 characters a second, 1 to 10 s a line, 2 s between transmissions). */
	constexpr float CharactersPerSecond = 10.f;
	constexpr float MinLine = 1.f;
	constexpr float MaxLine = 10.f;
	constexpr float Gap = 2.f;
	constexpr int32 MaxQueued = 3;
	constexpr double Tolerance = 1e-6;

	enum ECue : int32 { Brief, Lift, Loss, Res, SortieDocks, SortieAny, Long, Low, Evac, Low2 };

	FHollowRadioCue MakeCue(const TCHAR* Id, const FGameplayTag& Trigger, const EHollowRadioPriority Priority, const EHollowRadioScope Scope,
		const TArray<TArray<FString>>& Variants, const float Cooldown = 0.f, const float Stale = 0.f, const FName Sector = NAME_None)
	{
		FHollowRadioCue Cue;
		Cue.Id = Id;
		Cue.Trigger = Trigger;
		Cue.Priority = Priority;
		Cue.Scope = Scope;
		Cue.CooldownSeconds = Cooldown;
		Cue.StaleSeconds = Stale;
		Cue.Sector = Sector;
		for (const TArray<FString>& Lines : Variants)
		{
			FHollowRadioVariant& Variant = Cue.Variants.AddDefaulted_GetRef();
			for (const FString& Line : Lines)
			{
				Variant.Lines.Add({EHollowRadioSpeaker::Handler, FText::FromString(Line)});
			}
		}
		return Cue;
	}

	/** A script in memory, its cues in ECue order (the lines' lengths make their times: 20 characters = 2 s). */
	UHollowRadioScript* MakeScript()
	{
		using P = EHollowRadioPriority;
		using S = EHollowRadioScope;
		UHollowRadioScript* Script = NewObject<UHollowRadioScript>(GetTransientPackage());
		Script->HandlerName = FText::FromString(TEXT("HANDLER"));
		Script->EchoName = FText::FromString(TEXT("ECHO"));
		Script->CharactersPerSecond = CharactersPerSecond;
		Script->MinLineSeconds = MinLine;
		Script->MaxLineSeconds = MaxLine;
		Script->CueGapSeconds = Gap;
		Script->MaxQueued = MaxQueued;
		Script->Cues = {
			MakeCue(TEXT("Brief"), HollowTags::Radio_Hub_Arrival, P::High, S::Profile, {{TEXT("Briefing line one..."), TEXT("Briefing line two...")}}),
			MakeCue(TEXT("Lift"), HollowTags::Radio_Core_Lifted, P::Normal, S::Raid, {{TEXT("The core is lifted, carry it..")}, {TEXT("Core lifted, now to the zone..")}}),
			MakeCue(TEXT("Loss"), HollowTags::Radio_Squad_Loss, P::Critical, S::Repeat, {{TEXT("Minus one")}, {TEXT("Minus two")}, {TEXT("Minus 333")}}, 10.f),
			MakeCue(TEXT("Res"), HollowTags::Radio_Resonance_Critical, P::High, S::Repeat, {{TEXT("Resonance!")}}, 20.f, 5.f),
			MakeCue(TEXT("SortieDocks"), HollowTags::Radio_Raid_Sortie, P::High, S::Profile, {{TEXT("The docks.")}}, 0.f, 0.f, Docks),
			MakeCue(TEXT("SortieAny"), HollowTags::Radio_Raid_Sortie, P::High, S::Raid, {{TEXT("A sector.")}}),
			MakeCue(TEXT("Long"), HollowTags::Radio_Altar_Started, P::Low, S::Repeat, {{FString::ChrN(100, TEXT('a'))}}),
			MakeCue(TEXT("Low"), HollowTags::Radio_Stalker_Contact, P::Low, S::Repeat, {{TEXT("Low one.")}}),
			MakeCue(TEXT("Evac"), HollowTags::Radio_Extraction_Started, P::Normal, S::Raid, {{TEXT("Evac.")}}),
			MakeCue(TEXT("Low2"), HollowTags::Radio_Core_Sighted, P::Low, S::Repeat, {{TEXT("Low two.")}}),
		};
		return Script;
	}

	/** A local player's ExoFrame and its visor context in a test world. */
	struct FRig
	{
		FTestWorldWrapper World;
		APlayerController* Controller = nullptr;
		UHollowVisorContext* Context = nullptr;
		UHollowRadioModel* Radio = nullptr;
	};

	bool Build(FAutomationTestBase& Test, FRig& Rig)
	{
		UWorld* World = HollowGameTests::CreateWorld(Test, Rig.World);
		if (!World)
		{
			return false;
		}
		World->SpawnActor<AHollowGameState>()->DispatchBeginPlay();
		AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(Test, World);
		if (!Test.TestNotNull(TEXT("ExoFrame"), Character))
		{
			return false;
		}
		Test.AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
		Rig.Controller = HollowGameTests::PossessAsPlayer(World, Character, /*bLocalPlayer=*/true);
		Character->DispatchBeginPlay();
		Rig.Context = NewObject<UHollowVisorContext>(GetTransientPackage());
		Rig.Context->Initialize(Rig.Controller);
		Rig.Radio = Rig.Context->GetRadio();
		return Test.TestNotNull(TEXT("The context's radio"), Rig.Radio);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRadioQueueTest, "Hollow.UI.Radio.Queue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRadioQueueTest::RunTest(const FString& Parameters)
{
	// NAR-01a (ADR-0049): the radio's queue as pure rules, the time passed in.
	using namespace HollowRadioTests;
	UHollowRadioScript* Script = MakeScript();
	TSet<FName> Heard;
	const auto HeardForGood = [&Heard](const FName Id) { return Heard.Contains(Id); };
	const FGuid RaidA(1, 0, 0, 0);
	const FGuid RaidB(2, 0, 0, 0);
	FHollowRadioQueue Queue;
	Queue.SetScript(Script);
	const auto OnAir = [&Queue]() { return Queue.GetPlaying() ? Queue.GetPlaying()->Cue : INDEX_NONE; };
	const auto Text = [&Queue]() { return Queue.GetPlayingLine() ? Queue.GetPlayingLine()->Text.ToString() : FString(); };

	// The script's rules.
	TestEqual(TEXT("A sector's own cue first"), Script->FindCue(HollowTags::Radio_Raid_Sortie, Docks), static_cast<int32>(SortieDocks));
	TestEqual(TEXT("Else the cue for anywhere"), Script->FindCue(HollowTags::Radio_Raid_Sortie, Cathedral), static_cast<int32>(SortieAny));
	TestEqual(TEXT("No cue, nothing"), Script->FindCue(HollowTags::Radio, NAME_None), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("A short line stays the minimum"), Script->GetLineSeconds(FText::FromString(TEXT("Hi"))), MinLine);
	TestEqual(TEXT("20 characters at 10 a second: 2 s"), Script->GetLineSeconds(FText::FromString(FString::ChrN(20, TEXT('x')))), 2.f);
	TestEqual(TEXT("A long one stays the maximum"), Script->GetLineSeconds(FText::FromString(FString::ChrN(500, TEXT('x')))), MaxLine);

	// A Profile cue: on the air at once, its lines one after the other, remembered by the caller when heard to its end.
	TestEqual(TEXT("The briefing is queued"), Queue.Fire(HollowTags::Radio_Hub_Arrival, NAME_None, FGuid(), 0.0, HeardForGood), static_cast<int32>(Brief));
	FHollowRadioEvents Events = Queue.Advance(0.0);
	TestTrue(TEXT("It opens at once"), Events.bOpened && Events.bLineChanged && !Events.bClosed && OnAir() == Brief && Text() == TEXT("Briefing line one..."));
	TestTrue(TEXT("Its first line takes 2 s"), Queue.GetNextWake().IsSet() && FMath::IsNearlyEqual(Queue.GetNextWake().GetValue(), 2.0, Tolerance));
	Events = Queue.Advance(1.0);
	TestFalse(TEXT("Nothing changes meanwhile"), Events.bLineChanged);
	TestEqual(TEXT("Not twice at a time"), Queue.Fire(HollowTags::Radio_Hub_Arrival, NAME_None, FGuid(), 1.5, HeardForGood), static_cast<int32>(INDEX_NONE));
	Events = Queue.Advance(2.0);
	TestTrue(TEXT("The second line follows, no click between"), Events.bLineChanged && !Events.bOpened && !Events.bClosed && Text() == TEXT("Briefing line two..."));
	Events = Queue.Advance(4.0);
	TestTrue(TEXT("Heard to its end: closed, finished"), Events.bClosed && Events.Finished == TArray<int32>{Brief} && OnAir() == INDEX_NONE);
	Heard.Add(TEXT("Brief"));
	TestEqual(TEXT("A Profile cue heard is never queued again"), Queue.Fire(HollowTags::Radio_Hub_Arrival, NAME_None, FGuid(), 5.0, HeardForGood), static_cast<int32>(INDEX_NONE));
	Heard.Reset();
	TestEqual(TEXT("Nor twice in one visit"), Queue.Fire(HollowTags::Radio_Hub_Arrival, NAME_None, FGuid(), 5.0, HeardForGood), static_cast<int32>(INDEX_NONE));

	// The gap between transmissions; a Critical one cuts in and the cut one goes on afterwards.
	TestEqual(TEXT("A raid's cue is queued"), Queue.Fire(HollowTags::Radio_Core_Lifted, NAME_None, RaidA, 4.5, HeardForGood), static_cast<int32>(Lift));
	Events = Queue.Advance(4.5);
	TestTrue(TEXT("It waits for the gap"), !Events.bOpened && OnAir() == INDEX_NONE && Queue.GetNextWake().IsSet() && FMath::IsNearlyEqual(Queue.GetNextWake().GetValue(), 6.0, Tolerance));
	Events = Queue.Advance(6.0);
	const FString LiftText = Text();
	TestTrue(TEXT("Then it goes on the air (3 s)"), Events.bOpened && OnAir() == Lift && FMath::IsNearlyEqual(Queue.GetPlaying()->LineEnds, 9.0, Tolerance));
	TestEqual(TEXT("A Critical cue cuts in"), Queue.Fire(HollowTags::Radio_Squad_Loss, NAME_None, RaidA, 7.0, HeardForGood), static_cast<int32>(Loss));
	Events = Queue.Advance(7.0);
	const FString FirstLoss = Text();
	TestTrue(TEXT("The cut one closes, the Critical one opens at once"), Events.bClosed && Events.bOpened && OnAir() == Loss && Events.Finished.IsEmpty());
	TestTrue(TEXT("The cut one waits"), Queue.IsQueued(Lift));
	Events = Queue.Advance(8.0);
	TestTrue(TEXT("The Critical one is heard"), Events.Finished == TArray<int32>{Loss} && OnAir() == INDEX_NONE);
	Events = Queue.Advance(10.0);
	TestTrue(TEXT("The cut one goes on after the gap, from its line"), Events.bOpened && OnAir() == Lift && Text() == LiftText);
	TestEqual(TEXT("A Raid cue is heard once per raid"), Queue.Fire(HollowTags::Radio_Core_Lifted, NAME_None, RaidA, 11.0, HeardForGood), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Forced, but on the air: still one at a time"), Queue.Fire(HollowTags::Radio_Core_Lifted, NAME_None, RaidA, 11.0, HeardForGood, /*bForce=*/true),
		static_cast<int32>(INDEX_NONE));
	Queue.Advance(13.0);

	// Cooldowns, and a Repeat cue's variants taking turns.
	TestEqual(TEXT("A Repeat cue within its cooldown: nothing"), Queue.Fire(HollowTags::Radio_Squad_Loss, NAME_None, RaidA, 16.0, HeardForGood), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("After it: queued again"), Queue.Fire(HollowTags::Radio_Squad_Loss, NAME_None, RaidA, 17.5, HeardForGood), static_cast<int32>(Loss));
	Queue.Advance(17.5);
	TestTrue(TEXT("Its next variant"), OnAir() == Loss && Text() != FirstLoss);
	Queue.Advance(20.0);

	// A new raid forgets the last one; a forced cue ignores its scope.
	TestEqual(TEXT("Another raid hears its cue again"), Queue.Fire(HollowTags::Radio_Core_Lifted, NAME_None, RaidB, 30.0, HeardForGood), static_cast<int32>(Lift));
	Queue.Advance(30.0);
	Queue.Advance(40.0);
	TestEqual(TEXT("Forced, heard or not"), Queue.Fire(HollowTags::Radio_Core_Lifted, NAME_None, RaidB, 41.0, HeardForGood, /*bForce=*/true), static_cast<int32>(Lift));
	Queue.Clear();
	TestTrue(TEXT("Cleared: quiet, nothing waits"), !Queue.GetPlaying() && Queue.GetNumQueued() == 0 && !Queue.GetNextWake().IsSet());

	// Stale cues are dropped; a full queue makes room only for a more urgent cue; the most urgent goes first.
	FHollowRadioQueue Busy;
	Busy.SetScript(Script);
	Busy.Fire(HollowTags::Radio_Altar_Started, NAME_None, RaidA, 0.0, HeardForGood);
	Busy.Advance(0.0);
	TestTrue(TEXT("A 10 s line on the air"), Busy.GetPlaying() && Busy.GetPlaying()->Cue == Long && FMath::IsNearlyEqual(Busy.GetPlaying()->LineEnds, 10.0, Tolerance));
	TestEqual(TEXT("A cue that may wait 5 s"), Busy.Fire(HollowTags::Radio_Resonance_Critical, NAME_None, RaidA, 1.0, HeardForGood), static_cast<int32>(Res));
	Busy.Fire(HollowTags::Radio_Stalker_Contact, NAME_None, RaidA, 1.0, HeardForGood);
	Busy.Fire(HollowTags::Radio_Extraction_Started, NAME_None, RaidA, 1.5, HeardForGood);
	TestEqual(TEXT("Three wait"), Busy.GetNumQueued(), MaxQueued);
	TestEqual(TEXT("Full: a cue no more urgent than the least is refused"), Busy.Fire(HollowTags::Radio_Core_Sighted, NAME_None, RaidA, 2.0, HeardForGood), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("A more urgent one pushes the least urgent out"), Busy.Fire(HollowTags::Radio_Raid_Sortie, Docks, RaidA, 2.0, HeardForGood), static_cast<int32>(SortieDocks));
	Events = Busy.Advance(10.0);
	TestTrue(TEXT("The low one was pushed out"), Events.Dropped.Contains(Low) && !Busy.IsQueued(Low));
	TestTrue(TEXT("The one that waited too long is dropped"), Events.Dropped.Contains(Res) && !Busy.IsQueued(Res));
	TestTrue(TEXT("The long line is heard"), Events.Finished.Contains(Long));
	Events = Busy.Advance(12.0);
	TestTrue(TEXT("The most urgent waiting goes first"), Events.bOpened && Busy.GetPlaying() && Busy.GetPlaying()->Cue == SortieDocks);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRadioTriggersTest, "Hollow.UI.Radio.Triggers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRadioTriggersTest::RunTest(const FString& Parameters)
{
	// NAR-01a: what fires a radio trigger is a pure function of two snapshots of what this machine knows.
	using namespace HollowRadioTests;
	using T = TArray<FGameplayTag>;
	const auto Fires = [](const FHollowRadioFacts& Old, const FHollowRadioFacts& New) { return HollowRadioRules::TriggersFor(Old, New); };
	FHollowRadioFacts Hub;
	Hub.bInHub = true;
	TestTrue(TEXT("Nothing changed, nothing said"), Fires(Hub, Hub).IsEmpty());
	FHollowRadioFacts Standing = Hub;
	Standing.bHasBody = true;
	Standing.bAlive = true;
	TestTrue(TEXT("Standing in the hub: the arrival"), Fires(Hub, Standing) == T{HollowTags::Radio_Hub_Arrival});
	TestTrue(TEXT("Once"), Fires(Standing, Standing).IsEmpty());

	FHollowRadioFacts Assembling;
	Assembling.bInRaid = true;
	Assembling.RaidId = FGuid(7, 7, 7, 7);
	Assembling.bHasBody = true;
	Assembling.bAlive = true;
	Assembling.LivingPlayers = 3;
	FHollowRadioFacts Deploying = Assembling;
	Deploying.Phase = EHollowRaidPhase::Deploying;
	TestTrue(TEXT("No sortie before the manifest names the sector"), Fires(Assembling, Deploying).IsEmpty());
	Deploying.Sector = Docks;
	TestTrue(TEXT("Coming down into a named sector: the sortie"), Fires(Assembling, Deploying) == T{HollowTags::Radio_Raid_Sortie});
	FHollowRadioFacts InRaid = Deploying;
	InRaid.Phase = EHollowRaidPhase::InRaid;
	TestTrue(TEXT("Once per raid"), Fires(Deploying, InRaid).IsEmpty());
	TestTrue(TEXT("Even landing straight in the raid"), Fires(Assembling, InRaid) == T{HollowTags::Radio_Raid_Sortie});
	TestTrue(TEXT("Leaving the hub is no arrival"), Fires(Standing, InRaid) == T{HollowTags::Radio_Raid_Sortie});

	FHollowRadioFacts Seen = InRaid;
	Seen.bCoreSighted = true;
	TestTrue(TEXT("A core in its vault seen"), Fires(InRaid, Seen) == T{HollowTags::Radio_Core_Sighted});
	FHollowRadioFacts Lifted = Seen;
	Lifted.bCoreLifted = true;
	TestTrue(TEXT("Then lifted"), Fires(Seen, Lifted) == T{HollowTags::Radio_Core_Lifted});
	TestTrue(TEXT("Seen and lifted at once: the lift says it all"), Fires(InRaid, Lifted) == T{HollowTags::Radio_Core_Lifted});
	FHollowRadioFacts Ritual = Lifted;
	Ritual.bRitual = true;
	TestTrue(TEXT("A ritual starts"), Fires(Lifted, Ritual) == T{HollowTags::Radio_Altar_Started});
	FHollowRadioFacts Stalker = Ritual;
	Stalker.bStalkerSighted = true;
	TestTrue(TEXT("The stalker seen"), Fires(Ritual, Stalker) == T{HollowTags::Radio_Stalker_Contact});

	FHollowRadioFacts Critical = InRaid;
	Critical.Tier = EHollowResonanceTier::Critical;
	TestTrue(TEXT("Resonance reaches Critical"), Fires(InRaid, Critical) == T{HollowTags::Radio_Resonance_Critical});
	TestTrue(TEXT("Staying there says nothing"), Fires(Critical, Critical).IsEmpty());
	FHollowRadioFacts DeadCritical = Critical;
	DeadCritical.bAlive = false;
	TestTrue(TEXT("Not to the dead"), Fires(InRaid, DeadCritical).IsEmpty());

	FHollowRadioFacts Evac = InRaid;
	Evac.Phase = EHollowRaidPhase::Extraction;
	TestTrue(TEXT("The countdown starts"), Fires(InRaid, Evac) == T{HollowTags::Radio_Extraction_Started});

	FHollowRadioFacts Loss = InRaid;
	Loss.LivingPlayers = 2;
	TestTrue(TEXT("A squad mate dies"), Fires(InRaid, Loss) == T{HollowTags::Radio_Squad_Loss});
	FHollowRadioFacts Down = Loss;
	Down.bAlive = false;
	TestTrue(TEXT("This player's own death is no squad mate's"), Fires(InRaid, Down).IsEmpty());
	FHollowRadioFacts Landing = Deploying;
	Landing.LivingPlayers = 2;
	TestTrue(TEXT("Nor someone dropping out while the squad comes down"), Fires(Deploying, Landing).IsEmpty());
	FHollowRadioFacts Again = Loss;
	Again.RaidId = FGuid(8, 8, 8, 8);
	Again.bCoreLifted = true;
	TestTrue(TEXT("A new raid: none of the last one counts"), Fires(Lifted, Again) == (T{HollowTags::Radio_Raid_Sortie, HollowTags::Radio_Core_Lifted}));
	FHollowRadioFacts Log = InRaid;
	Log.NewLore = 1;
	TestTrue(TEXT("NAR-01b: a crew log new to this player"), Fires(InRaid, Log) == T{HollowTags::Radio_Lore_Found});
	TestTrue(TEXT("Once per log"), Fires(Log, Log).IsEmpty());

	// The sensors.
	TestTrue(TEXT("They look while the core and the stalker are unseen"), HollowRadioRules::ShouldSense(InRaid));
	TestFalse(TEXT("Not in the hub"), HollowRadioRules::ShouldSense(Standing));
	TestFalse(TEXT("Not while coming down"), HollowRadioRules::ShouldSense(Deploying));
	TestFalse(TEXT("Not once both were seen"), HollowRadioRules::ShouldSense(Stalker));
	TestFalse(TEXT("Not for the dead"), HollowRadioRules::ShouldSense(Down));
	const FVector Eye(0.0, 0.0, 0.0);
	const FVector Ahead(1.0, 0.0, 0.0);
	TestTrue(TEXT("Ahead and near: seen"), HollowRadioRules::IsInSight(Eye, Ahead, FVector(1000.0, 200.0, 0.0), 1500.f, 45.f));
	TestFalse(TEXT("Too far"), HollowRadioRules::IsInSight(Eye, Ahead, FVector(1600.0, 0.0, 0.0), 1500.f, 45.f));
	TestFalse(TEXT("Beside the view"), HollowRadioRules::IsInSight(Eye, Ahead, FVector(300.0, 600.0, 0.0), 1500.f, 45.f));
	TestFalse(TEXT("Behind"), HollowRadioRules::IsInSight(Eye, Ahead, FVector(-500.0, 0.0, 0.0), 1500.f, 45.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRadioDataTest, "Hollow.UI.Radio.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRadioDataTest::RunTest(const FString& Parameters)
{
	// The shipped DA_RadioScript: valid, a cue for every trigger, the briefing and each sector's first sortie heard once
	// for good, and every line readable in the time it stays, in both languages.
	using namespace HollowRadioTests;
	using namespace HollowLocalizationTests;
	const UHollowRadioScript* Script = GetDefault<UHollowUISettings>()->RadioScript.LoadSynchronous();
	if (!TestNotNull(TEXT("DA_RadioScript (UHollowUISettings::RadioScript)"), Script))
	{
		return false;
	}
#if WITH_EDITOR
	FDataValidationContext Validation;
	const EDataValidationResult Result = Script->IsDataValid(Validation);
	for (const FDataValidationContext::FIssue& Issue : Validation.GetIssues())
	{
		AddError(Issue.Message.ToString());
	}
	TestTrue(TEXT("DA_RadioScript is valid"), Result != EDataValidationResult::Invalid);
#endif

	const UGameplayTagsManager& Tags = UGameplayTagsManager::Get();
	for (const FGameplayTag& Trigger : Tags.RequestGameplayTagChildren(HollowTags::Radio))
	{
		if (!Tags.RequestGameplayTagChildren(Trigger).IsEmpty())
		{
			continue;   // a group (Hollow.Radio.Core): its leaves are the triggers
		}
		TestTrue(*FString::Printf(TEXT("%s has a cue"), *Trigger.ToString()), Script->FindCue(Trigger, NAME_None) != INDEX_NONE
			|| (Script->FindCue(Trigger, Docks) != INDEX_NONE && Script->FindCue(Trigger, Cathedral) != INDEX_NONE));
	}
	const int32 Brief = Script->FindCue(HollowTags::Radio_Hub_Arrival, NAME_None);
	TestTrue(TEXT("The hub briefing is heard once for good"), Brief != INDEX_NONE && Script->Cues[Brief].Scope == EHollowRadioScope::Profile
		&& Script->Cues[Brief].Id == Briefing);
	for (const FName Sector : {Docks, Cathedral})
	{
		const int32 Sortie = Script->FindCue(HollowTags::Radio_Raid_Sortie, Sector);
		TestTrue(*FString::Printf(TEXT("The first sortie into %s is its own, heard once for good"), *Sector.ToString()),
			Sortie != INDEX_NONE && Script->Cues[Sortie].Sector == Sector && Script->Cues[Sortie].Scope == EHollowRadioScope::Profile);
	}
	const int32 Loss = Script->FindCue(HollowTags::Radio_Squad_Loss, NAME_None);
	TestTrue(TEXT("A squad mate's death cuts in"), Loss != INDEX_NONE && Script->Cues[Loss].Priority == EHollowRadioPriority::Critical);
	const int32 Contact = Script->FindCue(HollowTags::Radio_Stalker_Contact, NAME_None);
	TestTrue(TEXT("The stalker first mimics the handler"), Contact != INDEX_NONE && !Script->Cues[Contact].Variants.ContainsByPredicate(
		[](const FHollowRadioVariant& Variant) { return Variant.Lines.IsEmpty() || Variant.Lines[0].Speaker != EHollowRadioSpeaker::Echo; }));

	const float Readable = Script->MaxLineSeconds * Script->CharactersPerSecond;
	for (const EHollowLanguage Language : {EHollowLanguage::English, EHollowLanguage::Russian})
	{
		const FScopedLanguage Scoped(Language);
		for (const FHollowRadioCue& Cue : Script->Cues)
		{
			for (const FHollowRadioVariant& Variant : Cue.Variants)
			{
				for (const FHollowRadioLine& Line : Variant.Lines)
				{
					const FString Shown = Line.Text.ToString();
					TestTrue(*FString::Printf(TEXT("%s, %s: \"%s\" is read in the %.0f s it stays"), *Cue.Id.ToString(), LexToString(Language), *Shown,
						Script->MaxLineSeconds), Shown.Len() <= Readable);
					TestTrue(*FString::Printf(TEXT("%s: \"%s\" is %s"), *Cue.Id.ToString(), *Shown, LexToString(Language)),
						(Language == EHollowLanguage::Russian) == HasCyrillic(Shown));
				}
			}
		}
		TestTrue(*FString::Printf(TEXT("Call signs in %s"), LexToString(Language)), !Script->HandlerName.IsEmpty() && !Script->EchoName.IsEmpty()
			&& (Language == EHollowLanguage::Russian) == HasCyrillic(Script->HandlerName.ToString()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRadioSubtitlesTest, "Hollow.UI.Radio.Subtitles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRadioSubtitlesTest::RunTest(const FString& Parameters)
{
	// The real W_Visor's subtitle, bound to a player's radio with the shipped script: the hub briefing opens with a click,
	// its lines follow at their pace, Russian at once on a language switch without a cut word, the close click, the
	// briefing remembered for good and never heard again.
	using namespace HollowRadioTests;
	using namespace HollowLocalizationTests;
	const FVector2D View(1920.0, 1080.0);
	FRig Rig;
	const TSubclassOf<UUserWidget> VisorClass = LoadClass<UUserWidget>(nullptr, VisorPath);
	if (!TestTrue(TEXT("Slate measures text here"), CanMeasureText()) || !TestNotNull(TEXT("W_Visor"), VisorClass.Get()) || !Build(*this, Rig))
	{
		return false;
	}
	double Clock = 100.0;
	TSet<FName> Heard;
	Rig.Radio->SetClockForTests([&Clock]() { return Clock; });
	Rig.Radio->SetHeardForTests(&Heard);
	const UHollowRadioScript* Script = Rig.Radio->GetScript();
	Rig.Radio->SetScriptForTests(nullptr);
	Rig.Radio->SetScriptForTests(Script);   // a fresh radio: nothing this world said before counts
	UUserWidget* Visor = CreateWidget<UUserWidget>(Rig.World.GetTestWorld(), VisorClass);
	UHollowRadioSubtitleWidget* Subtitle = Visor ? Cast<UHollowRadioSubtitleWidget>(Visor->WidgetTree->FindWidget(TEXT("RadioSubtitle"))) : nullptr;
	const UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(Rig.World.GetTestWorld());
	if (!TestNotNull(TEXT("The shipped radio script"), Script) || !TestNotNull(TEXT("W_Visor's RadioSubtitle"), Subtitle) || !TestNotNull(TEXT("Audio"), Audio))
	{
		Rig.Context->Deinitialize();
		return false;
	}
	Subtitle->SetContext(Rig.Context);
	TestFalse(TEXT("A quiet radio shows nothing"), Subtitle->IsShown());
	const int32 Opened = Audio->GetPlayCount(HollowTags::Sound_Radio_Open);
	const int32 Closed = Audio->GetPlayCount(HollowTags::Sound_Radio_Close);

	const int32 Brief = Script->FindCue(HollowTags::Radio_Hub_Arrival, NAME_None);
	if (!TestTrue(TEXT("The hub arrival is on the air"), Brief != INDEX_NONE && Rig.Radio->Fire(HollowTags::Radio_Hub_Arrival)))
	{
		Rig.Context->Deinitialize();
		return false;
	}
	const TArray<FHollowRadioLine>& Lines = Script->Cues[Brief].Variants[0].Lines;
	TestTrue(TEXT("Its first line, under the handler's call sign"), Subtitle->IsShown() && Subtitle->GetLineText().EqualTo(Lines[0].Text)
		&& Subtitle->GetSpeakerText().EqualTo(Script->HandlerName));
	TestEqual(TEXT("The opening click"), Audio->GetPlayCount(HollowTags::Sound_Radio_Open) - Opened, 1);
	for (const FString& Overflow : FindOverflows(*Visor, View))
	{
		AddError(TEXT("Cut in English: ") + Overflow);
	}
	Subtitle->SetContext(Rig.Context);   // laying the visor out built its Slate widgets anew (destructed: unbound)
	{
		const FScopedLanguage Russian(EHollowLanguage::Russian);
		TestTrue(TEXT("Russian at once, call sign and line"), HasCyrillic(Subtitle->GetLineText().ToString()) && HasCyrillic(Subtitle->GetSpeakerText().ToString()));
		for (const FString& Overflow : FindOverflows(*Visor, View))
		{
			AddError(TEXT("Cut in Russian: ") + Overflow);
		}
		Subtitle->SetContext(Rig.Context);
	}
	TestFalse(TEXT("English again"), HasCyrillic(Subtitle->GetLineText().ToString()));
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		Clock = Rig.Radio->GetQueue().GetNextWake().Get(Clock);
		Rig.Radio->Update();
		TestTrue(*FString::Printf(TEXT("Line %d follows"), Index + 1), Subtitle->IsShown() && Subtitle->GetLineText().EqualTo(Lines[Index].Text));
	}
	TestEqual(TEXT("No click between the lines"), Audio->GetPlayCount(HollowTags::Sound_Radio_Open) - Opened, 1);
	Clock = Rig.Radio->GetQueue().GetNextWake().Get(Clock);
	Rig.Radio->Update();
	TestFalse(TEXT("Over: the subtitle hides"), Subtitle->IsShown());
	TestEqual(TEXT("The closing click"), Audio->GetPlayCount(HollowTags::Sound_Radio_Close) - Closed, 1);
	TestTrue(TEXT("The briefing is remembered for good"), Heard.Contains(Briefing));
	Clock += 60.0;
	TestFalse(TEXT("And never heard again"), Rig.Radio->Fire(HollowTags::Radio_Hub_Arrival));
	Subtitle->SetContext(nullptr);
	Rig.Context->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowVisorLoreNoticeTest, "Hollow.UI.Visor.LoreNotice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowVisorLoreNoticeTest::RunTest(const FString& Parameters)
{
	// NAR-01b (ADR-0050): the real W_Visor's notice follows the raid's found logs: a log downloaded on the server shows
	// "CREW LOG 1/12", its title and its bonus, in Russian at once on a language switch, and hides after its time; a log
	// already in the archive says so instead of the bonus.
	using namespace HollowRadioTests;
	using namespace HollowLocalizationTests;
	FRig Rig;
	const TSubclassOf<UUserWidget> VisorClass = LoadClass<UUserWidget>(nullptr, VisorPath);
	const TArray<const UHollowLoreEntryDefinition*> Entries = UHollowLoreEntryDefinition::LoadAll();
	if (!TestNotNull(TEXT("W_Visor"), VisorClass.Get()) || !TestEqual(TEXT("Twelve logs"), Entries.Num(), 12) || !Build(*this, Rig))
	{
		return false;
	}
	UUserWidget* Visor = CreateWidget<UUserWidget>(Rig.World.GetTestWorld(), VisorClass);
	UHollowLoreNoticeWidget* Notice = Visor ? Cast<UHollowLoreNoticeWidget>(Visor->WidgetTree->FindWidget(TEXT("LoreNotice"))) : nullptr;
	UHollowRaidStateComponent* Raid = UHollowRaidStateComponent::Get(Rig.World.GetTestWorld());
	const UHollowVisorStyle* Style = Rig.Context->GetStyle();
	if (!TestNotNull(TEXT("W_Visor's LoreNotice"), Notice) || !TestNotNull(TEXT("The raid state"), Raid) || !TestNotNull(TEXT("The visor style"), Style))
	{
		Rig.Context->Deinitialize();
		return false;
	}
	Notice->SetContext(Rig.Context);
	TestFalse(TEXT("Nothing downloaded: hidden"), Notice->IsShown());
	const UHollowLoreEntryDefinition* Entry = Entries[2];
	Raid->AddLoreFound(Entry->GetPrimaryAssetId());   // the server's download (a test world is its own server)
	TestTrue(TEXT("Shown: CREW LOG 1/12, the title, the bonus"), Notice->IsShown() && Notice->GetTitleText().ToString().Contains(TEXT("1/12"))
		&& Notice->GetEntryText().EqualTo(Entry->Title) && Notice->GetDetailText().ToString().Contains(TEXT("75")));
	{
		const FScopedLanguage Russian(EHollowLanguage::Russian);
		TestTrue(TEXT("Russian at once"), HasCyrillic(Notice->GetTitleText().ToString()) && HasCyrillic(Notice->GetEntryText().ToString())
			&& HasCyrillic(Notice->GetDetailText().ToString()));
		for (const FString& Overflow : FindOverflows(*Visor, FVector2D(1920.0, 1080.0)))
		{
			AddError(TEXT("Cut in Russian: ") + Overflow);
		}
	}
	Notice->SetContext(Rig.Context);   // laying the visor out built its Slate widgets anew
	Notice->ShowLore(Entries[0]->GetPrimaryAssetId(), /*bNew=*/false);
	TestTrue(TEXT("A log already in the archive: no bonus"), Notice->IsShown() && Notice->GetDetailText().EqualTo(Style->LoreNoticeKnown));
	for (float Elapsed = 0.f; Elapsed < Style->LoreNoticeSeconds + 0.5f; Elapsed += 0.1f)
	{
		++GFrameCounter;   // the timer manager ticks once per frame counter
		Rig.World.TickTestWorld(0.1f);
	}
	TestFalse(TEXT("Hidden after its time"), Notice->IsShown());
	Notice->SetContext(nullptr);
	Rig.Context->Deinitialize();
	return true;
}

#endif
