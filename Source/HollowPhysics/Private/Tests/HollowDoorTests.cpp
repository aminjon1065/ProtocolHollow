#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Collision/HollowCollisionChannels.h"
#include "Components/StaticMeshComponent.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorRules.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

namespace HollowDoorTests
{
	// Test data (the shipped numbers live in DA_Door_Standard).
	constexpr float WeldPerWork = 0.04f;
	constexpr float CutPerWork = 0.02f;
	constexpr float Seal = 0.15f;
	constexpr float OpenSeconds = 1.f;
	constexpr float Passable = 0.3f;
	constexpr float ReplayWindow = 1.f;
	constexpr float SendInterval = 0.2f;
	constexpr int32 NetStateBits = 64;

	const TCHAR* const DoorClass = TEXT("/Game/Hollow/World/Kit/Doors/BP_Door.BP_Door_C");
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float WorkPerSample = 1.f;   // a tool at 10 work units/s sampled at 10 Hz
	constexpr double PawnHeight = 100.0;   // capsule centre of a pawn standing in the doorway
	constexpr float StructurePoints = 300.f;
	constexpr float WeldAbsorb = 600.f;
	constexpr float Blow = 120.f;

	UHollowDoorDefinition* MakeDefinition()
	{
		UHollowDoorDefinition* Definition = NewObject<UHollowDoorDefinition>(GetTransientPackage());
		Definition->WeldToolTag = HollowTags::Ability_Tool_Welder;
		Definition->CutToolTag = HollowTags::Ability_Tool_PlasmaCutter;
		Definition->WeldPerWorkUnit = WeldPerWork;
		Definition->CutPerWorkUnit = CutPerWork;
		Definition->SealThreshold = Seal;
		Definition->OpenDuration = OpenSeconds;
		Definition->OpenEaseExponent = 2.f;
		Definition->PassableFraction = Passable;
		Definition->TransitionReplayWindow = ReplayWindow;
		Definition->WeldSendInterval = SendInterval;
		Definition->StructureHitPoints = StructurePoints;
		Definition->WeldAbsorbFactor = WeldAbsorb;
		return Definition;
	}

	FHollowDoorServerState Door(const EHollowDoorState State, const float Weld = 0.f)
	{
		FHollowDoorServerState Server;
		Server.Net.State = State;
		Server.Weld = Weld;
		Server.Net.WeldQ = HollowDoorRules::Quantize(Weld);
		return Server;
	}

	FHollowDoorEvent Event(const EHollowDoorEvent Type, const float Amount = 0.f, const bool bClear = true, const float Time = 0.f)
	{
		FHollowDoorEvent Result;
		Result.Type = Type;
		Result.Amount = Amount;
		Result.bDoorwayClear = bClear;
		Result.ServerTime = Time;
		return Result;
	}

	FHollowToolWork Work(const FGameplayTag& Tool, const float Amount)
	{
		FHollowToolWork Result;
		Result.ToolTag = Tool;
		Result.Amount = Amount;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDoorWeldClampTest, "Hollow.Physics.Door.WeldClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDoorWeldClampTest::RunTest(const FString& Parameters)
{
	using namespace HollowDoorTests;
	using namespace HollowDoorRules;
	const UHollowDoorDefinition* Definition = MakeDefinition();
	constexpr float Huge = 1.e6f;

	TestEqual(TEXT("Welding never exceeds 1"), ApplyWeld(0.9f, Huge, *Definition), 1.f);
	TestEqual(TEXT("Negative work does not unweld"), ApplyWeld(0.5f, -Huge, *Definition), 0.5f);
	TestEqual(TEXT("Weld per work unit"), ApplyWeld(0.f, 2.f, *Definition), 2.f * WeldPerWork);
	TestEqual(TEXT("Cutting never goes below 0"), ApplyCut(0.1f, Huge, *Definition), 0.f);
	TestEqual(TEXT("Cut per work unit"), ApplyCut(1.f, 10.f, *Definition), 1.f - 10.f * CutPerWork);

	TestEqual(TEXT("Quantize 0"), static_cast<int32>(Quantize(0.f)), 0);
	TestEqual(TEXT("Quantize 1"), static_cast<int32>(Quantize(1.f)), 255);
	TestEqual(TEXT("Quantize clamps below"), static_cast<int32>(Quantize(-1.f)), 0);
	TestEqual(TEXT("Quantize clamps above"), static_cast<int32>(Quantize(2.f)), 255);
	float WorstError = 0.f;
	constexpr int32 Steps = 1000;
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const float Value = static_cast<float>(Step) / Steps;
		WorstError = FMath::Max(WorstError, FMath::Abs(Dequantize(Quantize(Value)) - Value));
	}
	TestTrue(FString::Printf(TEXT("Quantization error is at most half a step (%.5f)"), WorstError), WorstError <= 0.5f / 255.f + KINDA_SMALL_NUMBER);

	TestFalse(TEXT("Unchanged weld is not sent"), ShouldSendWeld(10, 10, SendInterval * 2, SendInterval));
	TestTrue(TEXT("A changed weld is sent once the interval passed (inclusive)"), ShouldSendWeld(10, 11, SendInterval, SendInterval));
	TestFalse(TEXT("...not before"), ShouldSendWeld(10, 11, SendInterval * 0.5, SendInterval));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDoorTransitionsTest, "Hollow.Physics.Door.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDoorTransitionsTest::RunTest(const FString& Parameters)
{
	using namespace HollowDoorTests;
	using EState = EHollowDoorState;
	using EOutcome = EHollowDoorOutcome;
	const UHollowDoorDefinition* Def = MakeDefinition();
	constexpr float Now = 42.f;

	// Row 1: open.
	FHollowDoorServerState D = Door(EState::Closed);
	TestTrue(TEXT("Closed + Interact -> Opening"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Interact, 0.f, true, Now), *Def) == EOutcome::StateChanged
		&& D.Net.State == EState::Opening && D.Net.TransitionSerial == 1 && D.Net.StateServerTime == Now);
	TestTrue(TEXT("Opening + OpenFinished -> Open"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::OpenFinished), *Def) == EOutcome::StateChanged && D.Net.State == EState::Open);
	D = Door(EState::Opening);
	TestTrue(TEXT("Interact while moving is ignored"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Interact), *Def) == EOutcome::Rejected);

	// Row 2: close only with a free doorway.
	D = Door(EState::Open);
	TestTrue(TEXT("Open + Interact with someone in the doorway is refused"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Interact, 0.f, false), *Def) == EOutcome::Rejected && D.Net.State == EState::Open);
	TestTrue(TEXT("Open + Interact -> Closing"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Interact), *Def) == EOutcome::StateChanged && D.Net.State == EState::Closing);
	TestTrue(TEXT("Closing + CloseFinished -> Closed"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::CloseFinished), *Def) == EOutcome::StateChanged && D.Net.State == EState::Closed);
	TestTrue(TEXT("OpenFinished outside Opening is refused"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::OpenFinished), *Def) == EOutcome::Rejected);

	// Locked doors stay shut.
	D = Door(EState::Closed);
	D.Net.Flags = static_cast<uint8>(EHollowDoorFlags::Locked);
	TestTrue(TEXT("Locked + Interact is refused"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Interact), *Def) == EOutcome::Rejected);

	// Row 3: weld below the threshold keeps the door closed, crossing it seals.
	D = Door(EState::Closed);
	const uint8 SerialBefore = D.Net.TransitionSerial;
	TestTrue(TEXT("A partial weld stays Closed"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Weld, WorkPerSample), *Def) == EOutcome::WeldChanged
		&& D.Net.State == EState::Closed && D.Net.TransitionSerial == SerialBefore);
	const float ToSeal = (Seal - D.Weld) / WeldPerWork;
	TestTrue(TEXT("Reaching SealThreshold -> Welded"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Weld, ToSeal), *Def) == EOutcome::StateChanged
		&& D.Net.State == EState::Welded && D.Net.WeldQ == HollowDoorRules::Quantize(D.Weld));

	// Row 4: welding a welded door strengthens it up to 1.
	TestTrue(TEXT("Welded + Weld -> stronger"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Weld, WorkPerSample), *Def) == EOutcome::WeldChanged);
	HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Weld, 1.e3f), *Def);
	TestTrue(TEXT("...and stays at 1"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Weld, WorkPerSample), *Def) == EOutcome::Unchanged && D.Weld == 1.f);
	TestTrue(TEXT("Welded + Interact is refused"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Interact), *Def) == EOutcome::Rejected);

	// Rows 5-6: cutting weakens, and at 0 the door is closed again.
	TestTrue(TEXT("Welded + Cut -> weaker"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Cut, WorkPerSample), *Def) == EOutcome::WeldChanged && D.Net.State == EState::Welded);
	TestTrue(TEXT("Cut to 0 -> Closed"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Cut, 1.e3f), *Def) == EOutcome::StateChanged && D.Net.State == EState::Closed && D.Weld == 0.f);
	TestTrue(TEXT("Cutting a closed door without a weld is refused"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Cut, WorkPerSample), *Def) == EOutcome::Rejected);

	// Partial welds: the cutter removes them, opening breaks them.
	D = Door(EState::Closed, Seal * 0.5f);
	TestTrue(TEXT("Cutting a partial weld"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Cut, WorkPerSample), *Def) == EOutcome::WeldChanged);
	TestTrue(TEXT("Opening breaks a partial weld"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Interact), *Def) == EOutcome::StateChanged && D.Weld == 0.f && D.Net.WeldQ == 0);

	// Tools do nothing on open or moving doors, nothing works on a breached one.
	for (const EState State : {EState::Open, EState::Opening, EState::Closing, EState::Breached})
	{
		D = Door(State);
		TestTrue(TEXT("Weld refused on open, moving or breached doors"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Weld, WorkPerSample), *Def) == EOutcome::Rejected);
		TestTrue(TEXT("Cut refused on open, moving or breached doors"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Cut, WorkPerSample), *Def) == EOutcome::Rejected);
	}
	D = Door(EState::Breached);
	TestTrue(TEXT("Breached is terminal"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Interact), *Def) == EOutcome::Rejected);

	// Accepts is a dry run.
	D = Door(EState::Closed);
	const FHollowDoorServerState Before = D;
	TestTrue(TEXT("Accepts says yes"), HollowDoorRules::Accepts(D, Event(EHollowDoorEvent::Interact), *Def));
	TestTrue(TEXT("...without changing anything"), D.Net == Before.Net && D.Weld == Before.Weld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDoorNetStateRoundTripTest, "Hollow.Physics.Door.NetStateRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDoorNetStateRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace HollowDoorTests;
	bool bAllEqual = true;
	bool bAllSized = true;
	for (const EHollowDoorState State : {EHollowDoorState::Open, EHollowDoorState::Opening, EHollowDoorState::Closing, EHollowDoorState::Closed,
		EHollowDoorState::Welded, EHollowDoorState::Breached})
	{
		for (const uint8 Flags : {uint8(0), uint8(EHollowDoorFlags::Powered),
			uint8(EHollowDoorFlags::Powered | EHollowDoorFlags::Locked | EHollowDoorFlags::Barricaded | EHollowDoorFlags::BreachedBackwards)})
		{
			for (const uint8 Edge : {uint8(0), uint8(1), uint8(127), uint8(254), uint8(255)})
			{
				FHollowDoorNetState Sent;
				Sent.State = State;
				Sent.Flags = Flags;
				Sent.WeldQ = Edge;
				Sent.StructureQ = static_cast<uint8>(255 - Edge);
				Sent.TransitionSerial = Edge;
				Sent.StateServerTime = 1234.567f + Edge;

				FBitWriter Writer(0, /*bAllowResize=*/true);
				bool bOk = false;
				Sent.NetSerialize(Writer, nullptr, bOk);
				FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
				FHollowDoorNetState Received;
				Received.NetSerialize(Reader, nullptr, bOk);
				bAllEqual &= bOk && Received == Sent;
				bAllSized &= Writer.GetNumBits() == NetStateBits;
			}
		}
	}
	TestTrue(TEXT("Every state, flag set and edge value survives NetSerialize"), bAllEqual);
	TestTrue(TEXT("The net state is 8 bytes"), bAllSized);

	// Presentation from the replicated state.
	const UHollowDoorDefinition* Def = MakeDefinition();
	FHollowDoorNetState State;
	State.StateServerTime = 10.f;
	State.State = EHollowDoorState::Closed;
	TestEqual(TEXT("Closed leaf"), HollowDoorRules::LeafAlpha(State, 99.0, *Def), 0.f);
	State.State = EHollowDoorState::Open;
	TestEqual(TEXT("Open leaf"), HollowDoorRules::LeafAlpha(State, 99.0, *Def), 1.f);
	State.State = EHollowDoorState::Opening;
	TestEqual(TEXT("Opening starts closed"), HollowDoorRules::LeafAlpha(State, 10.0, *Def), 0.f);
	TestTrue(TEXT("Opening is half way at half time"), FMath::IsNearlyEqual(HollowDoorRules::LeafAlpha(State, 10.0 + OpenSeconds * 0.5, *Def), 0.5f, KINDA_SMALL_NUMBER));
	TestEqual(TEXT("Opening ends open, even when replicated late"), HollowDoorRules::LeafAlpha(State, 10.0 + OpenSeconds * 5.0, *Def), 1.f);
	State.State = EHollowDoorState::Closing;
	TestEqual(TEXT("Closing starts open"), HollowDoorRules::LeafAlpha(State, 10.0, *Def), 1.f);
	TestTrue(TEXT("Leaf blocks while barely open"), HollowDoorRules::IsLeafBlocking(EHollowDoorState::Opening, Passable * 0.5f, *Def));
	TestFalse(TEXT("...and lets pawns through past PassableFraction"), HollowDoorRules::IsLeafBlocking(EHollowDoorState::Opening, Passable, *Def));
	TestTrue(TEXT("Closing always blocks"), HollowDoorRules::IsLeafBlocking(EHollowDoorState::Closing, 1.f, *Def));
	TestFalse(TEXT("Open never blocks"), HollowDoorRules::IsLeafBlocking(EHollowDoorState::Open, 1.f, *Def));

	FHollowDoorNetState Old = State;
	FHollowDoorNetState New = State;
	New.TransitionSerial = Old.TransitionSerial + 1;
	New.StateServerTime = 50.f;
	TestTrue(TEXT("A recent transition is played"), HollowDoorRules::IsFreshTransition(Old, New, 50.0 + ReplayWindow * 0.5, *Def));
	TestFalse(TEXT("An old one snaps (late join, regained relevancy)"), HollowDoorRules::IsFreshTransition(Old, New, 50.0 + ReplayWindow * 2.0, *Def));
	TestFalse(TEXT("A weld-only update is no transition"), HollowDoorRules::IsFreshTransition(Old, Old, 0.0, *Def));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDoorStructuralTest, "Hollow.Physics.Door.Structural",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDoorStructuralTest::RunTest(const FString& Parameters)
{
	// Rows 7-8 (Docs/Architecture/03 sec. 3.3): the weld soaks Weld x WeldAbsorbFactor first, then the structure;
	// the state stays until the structure is gone, then Breached is final.
	using namespace HollowDoorTests;
	using EState = EHollowDoorState;
	using EOutcome = EHollowDoorOutcome;
	const UHollowDoorDefinition* Def = MakeDefinition();

	FHollowStructuralResult R = HollowDoorRules::ApplyStructural(1.f, 1.f, Blow, *Def);
	TestTrue(TEXT("A full weld soaks the blow"), FMath::IsNearlyEqual(R.Weld, 1.f - Blow / WeldAbsorb) && R.Structure == 1.f);
	R = HollowDoorRules::ApplyStructural(0.1f, 1.f, Blow, *Def);
	TestTrue(TEXT("What the weld cannot soak goes to the structure"), R.Weld == 0.f && FMath::IsNearlyEqual(R.Structure, 1.f - (Blow - 0.1f * WeldAbsorb) / StructurePoints));
	R = HollowDoorRules::ApplyStructural(0.f, 0.1f, Blow * 10.f, *Def);
	TestTrue(TEXT("Clamped at 0"), R.Weld == 0.f && R.Structure == 0.f);
	R = HollowDoorRules::ApplyStructural(0.5f, 0.5f, -Blow, *Def);
	TestTrue(TEXT("A negative blow heals nothing"), R.Weld == 0.5f && R.Structure == 0.5f);

	// A fully welded door: ceil((WeldAbsorbFactor + StructureHitPoints) / Blow) blows, still welded until the last.
	FHollowDoorServerState D = Door(EState::Welded, 1.f);
	FHollowDoorEvent Hit = Event(EHollowDoorEvent::Structural, Blow);
	Hit.bBackwards = true;
	int32 Blows = 0;
	EOutcome Outcome = EOutcome::Unchanged;
	while (D.Net.State == EState::Welded && Blows < 100)
	{
		Outcome = HollowDoorRules::Apply(D, Hit, *Def);
		++Blows;
		if (D.Net.State == EState::Welded)
		{
			TestTrue(TEXT("Every blow before the last is a structure change"), Outcome == EOutcome::StructureChanged);
		}
	}
	TestEqual(TEXT("Blows to breach a full weld"), Blows, FMath::CeilToInt((WeldAbsorb + StructurePoints) / Blow));
	TestTrue(TEXT("The last blow breaches"), Outcome == EOutcome::StateChanged && D.Net.State == EState::Breached && D.Structure == 0.f && D.Net.StructureQ == 0);
	TestTrue(TEXT("The blow's direction is kept for the torn leaf"), D.Net.HasFlag(EHollowDoorFlags::BreachedBackwards));
	TestTrue(TEXT("Breached takes no more blows"), HollowDoorRules::Apply(D, Hit, *Def) == EOutcome::Rejected);

	// A bare closed door goes faster, open ones too; StructureQ follows the exact structure.
	D = Door(EState::Closed);
	TestTrue(TEXT("A bare door dents"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Structural, Blow), *Def) == EOutcome::StructureChanged
		&& D.Net.State == EState::Closed && D.Net.StructureQ == HollowDoorRules::Quantize(1.f - Blow / StructurePoints));
	TestTrue(TEXT("A dented door still opens"), HollowDoorRules::Accepts(D, Event(EHollowDoorEvent::Interact), *Def));
	D = Door(EState::Open);
	TestTrue(TEXT("An open door breaks too"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Structural, StructurePoints), *Def) == EOutcome::StateChanged
		&& D.Net.State == EState::Breached && !D.Net.HasFlag(EHollowDoorFlags::BreachedBackwards));
	D = Door(EState::Closed);
	TestTrue(TEXT("A zero blow is refused"), HollowDoorRules::Apply(D, Event(EHollowDoorEvent::Structural, 0.f), *Def) == EOutcome::Rejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDoorBreachActorTest, "Hollow.Physics.Door.BreachActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDoorBreachActorTest::RunTest(const FString& Parameters)
{
	// The real BP_Door as an IHollowStructural: what an enemy sees (GetPassage) per state, blows from behind, the leaf
	// gone everywhere once breached and a cosmetic copy thrown along the blow.
	using namespace HollowDoorTests;
	FTestWorldWrapper TestWorld;
	const TSubclassOf<AHollowDoor> Class = LoadClass<AHollowDoor>(nullptr, DoorClass);
	if (!TestNotNull(TEXT("BP_Door"), Class.Get()) || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	AHollowDoor* Door = World->SpawnActor<AHollowDoor>(Class, FTransform::Identity);
	Door->DispatchBeginPlay();
	UHollowDoorStateComponent* State = Door->GetDoorState();
	const UHollowDoorDefinition* Def = State->GetDefinition();
	if (!TestNotNull(TEXT("Door definition"), Def) || !TestTrue(TEXT("DA_Door_Standard has a structure"), Def->StructureHitPoints > 0.f && Def->WeldAbsorbFactor > 0.f))
	{
		return false;
	}
	const IHollowStructural* Structural = Door;
	TestTrue(TEXT("Closed: an enemy opens it"), Structural->GetPassage() == EHollowPassage::Open);
	State->ApplyTool(EHollowDoorEvent::Weld, 1.f / Def->WeldPerWorkUnit);
	TestTrue(TEXT("Welded: an enemy has to break it"), State->GetNetState().State == EHollowDoorState::Welded && Structural->GetPassage() == EHollowPassage::Breach);

	// Blows from the far side (+X towards -X) until it gives way.
	FHollowStructuralHit Hit;
	Hit.Amount = Blow;
	Hit.Direction = -Door->GetActorForwardVector();
	Hit.DamageType = HollowTags::Damage_Structural;
	int32 Blows = 0;
	while (Door->CanTakeStructuralDamage() && Blows < 100)
	{
		TestTrue(TEXT("Each blow changes the door"), Door->ApplyStructuralDamage(Hit));
		++Blows;
	}
	TestEqual(TEXT("Blows to breach the shipped door"), Blows, FMath::CeilToInt((Def->WeldAbsorbFactor + Def->StructureHitPoints) / Blow));
	TestTrue(TEXT("Breached: clear for everyone"), State->GetNetState().State == EHollowDoorState::Breached && Structural->GetPassage() == EHollowPassage::Clear);
	TestTrue(TEXT("Breached backwards"), State->GetNetState().HasFlag(EHollowDoorFlags::BreachedBackwards));
	TestFalse(TEXT("The leaf is gone"), Door->GetLeaf()->IsVisible());
	TestTrue(TEXT("...and blocks nothing"), Door->GetLeaf()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	const UStaticMeshComponent* Torn = Door->GetTornLeaf();
	if (TestNotNull(TEXT("A torn leaf flies off"), Torn))
	{
		TestTrue(TEXT("It simulates on the debris channel"), Torn->IsSimulatingPhysics() && Torn->GetCollisionObjectType() == HollowCollision::Debris);
		TestTrue(TEXT("...and never blocks pawns"), Torn->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);
		TestTrue(TEXT("It flies along the blow"), FVector::DotProduct(Torn->GetPhysicsLinearVelocity(), Door->GetActorForwardVector()) < 0.0);
	}
	TestFalse(TEXT("Nothing more to break"), Door->ApplyStructuralDamage(Hit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDoorActorTest, "Hollow.Physics.Door.Actor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDoorActorTest::RunTest(const FString& Parameters)
{
	using namespace HollowDoorTests;
	FTestWorldWrapper TestWorld;
	const TSubclassOf<AHollowDoor> Class = LoadClass<AHollowDoor>(nullptr, DoorClass);
	if (!TestNotNull(TEXT("BP_Door"), Class.Get()) || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	AHollowDoor* Door = World->SpawnActor<AHollowDoor>(Class, FTransform::Identity);
	Door->DispatchBeginPlay();
	UHollowDoorStateComponent* State = Door->GetDoorState();
	const UHollowDoorDefinition* Def = State->GetDefinition();
	if (!TestNotNull(TEXT("Door definition"), Def))
	{
		return false;
	}
	auto Advance = [&TestWorld](const float Seconds)
	{
		// One engine frame per step: the timer manager ticks once per GFrameCounter.
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			TestWorld.TickTestWorld(TickSeconds);
		}
	};
	auto LeafBlocks = [Door]() { return Door->GetLeaf()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block; };
	const FGameplayTag Welder = Def->WeldToolTag;
	const FGameplayTag Cutter = Def->CutToolTag;

	TestTrue(TEXT("Starts closed and blocking"), State->GetNetState().State == EHollowDoorState::Closed && LeafBlocks());
	TestFalse(TEXT("A cutter has nothing to cut"), Door->CanAcceptToolWork(Work(Cutter, WorkPerSample)));

	// Open: the leaf follows server time.
	TestTrue(TEXT("Closed doors can be opened"), Door->CanInteract(nullptr));
	Door->Interact(nullptr);
	Advance(Def->OpenDuration * 0.5f);
	TestTrue(TEXT("Half way through opening"), State->GetNetState().State == EHollowDoorState::Opening && Door->GetLeafAlpha() > 0.f && Door->GetLeafAlpha() < 1.f);
	Advance(Def->OpenDuration * 0.5f + TickSeconds * 4.f);
	TestTrue(TEXT("Open, leaf up, passable"), State->GetNetState().State == EHollowDoorState::Open && Door->GetLeafAlpha() == 1.f && !LeafBlocks());
	TestFalse(TEXT("No welding an open door"), Door->CanAcceptToolWork(Work(Welder, WorkPerSample)));

	// Close: refused while a pawn stands in the doorway.
	ACharacter* Blocker = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FTransform(FVector(0.0, 0.0, PawnHeight)));
	TestFalse(TEXT("The doorway is occupied"), Door->IsDoorwayClear());
	Door->Interact(nullptr);
	TestTrue(TEXT("Closing on a pawn is refused"), State->GetNetState().State == EHollowDoorState::Open);
	Blocker->Destroy();
	Advance(TickSeconds);
	Door->Interact(nullptr);
	Advance(Def->OpenDuration + TickSeconds * 4.f);
	TestTrue(TEXT("Closed again"), State->GetNetState().State == EHollowDoorState::Closed && Door->GetLeafAlpha() == 0.f && LeafBlocks());

	// Weld it shut, then cut it open.
	int32 Samples = 0;
	while (State->GetNetState().State == EHollowDoorState::Closed && Door->CanAcceptToolWork(Work(Welder, WorkPerSample)) && Samples < 100)
	{
		Door->ApplyToolWork(Work(Welder, WorkPerSample));
		++Samples;
	}
	TestEqual(TEXT("Welded after SealThreshold / WeldPerWorkUnit samples"), Samples, FMath::CeilToInt(Def->SealThreshold / Def->WeldPerWorkUnit));
	TestTrue(TEXT("Welded doors cannot be opened"), State->GetNetState().State == EHollowDoorState::Welded && !Door->CanInteract(nullptr));
	Door->ApplyToolWork(Work(Welder, 1.e3f));
	TestEqual(TEXT("Weld clamps at 1"), State->GetWeld(), 1.f);
	Advance(Def->WeldSendInterval * 2.f);
	TestEqual(TEXT("The final weld value reaches the replicated state"), static_cast<int32>(State->GetNetState().WeldQ), 255);
	Samples = 0;
	while (State->GetNetState().State == EHollowDoorState::Welded && Samples < 1000)
	{
		Door->ApplyToolWork(Work(Cutter, WorkPerSample));
		++Samples;
	}
	const int32 ExpectedCuts = FMath::CeilToInt(1.f / Def->CutPerWorkUnit);   // float steps may need one more
	TestTrue(FString::Printf(TEXT("Cut open after 1 / CutPerWorkUnit samples (%d, expected %d)"), Samples, ExpectedCuts), Samples == ExpectedCuts || Samples == ExpectedCuts + 1);
	TestTrue(TEXT("Closed, weld gone"), State->GetNetState().State == EHollowDoorState::Closed && State->GetWeld() == 0.f && State->GetNetState().WeldQ == 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
