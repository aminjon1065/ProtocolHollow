#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Carry/HollowCarryPolicy.h"
#include "Carry/HollowCarryTypes.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Movement/HollowMovementTuning.h"

namespace HollowCarryTests
{
	// Test data only: real values live in DA_MovementTuning_ExoFrame and DefaultGame.ini.
	constexpr float Grace = 0.25f;
	constexpr float SoloScale = 0.45f;
	constexpr float CoopScale = 0.75f;
	constexpr float DraggedScale = 0.25f;
	constexpr float DroneScale = 0.75f;
	constexpr double Long = 10.0;

	FHollowCarryMovementTuning MakeTuning()
	{
		FHollowCarryMovementTuning Tuning;
		Tuning.Solo.SpeedScale = SoloScale;
		Tuning.Coop.SpeedScale = CoopScale;
		Tuning.Dragged.SpeedScale = DraggedScale;
		Tuning.Drone.SpeedScale = DroneScale;
		return Tuning;
	}

	FHollowServerCarry Server(const EHollowCarryClaim Current, const EHollowCarryClaim Previous)
	{
		FHollowServerCarry Result;
		Result.Current = Current;
		Result.Previous = Previous;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarrySlowdownPolicyTest, "Hollow.Physics.Carry.SlowdownPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarrySlowdownPolicyTest::RunTest(const FString& Parameters)
{
	using namespace HollowCarryTests;
	using EClaim = EHollowCarryClaim;
	const FHollowCarryMovementTuning Tuning = MakeTuning();
	auto Effective = [&Tuning](const EClaim Claimed, const FHollowServerCarry& State, const double Age)
	{
		return HollowCarryPolicy::Effective(Claimed, State, Age, Grace, Tuning);
	};

	// Severity follows the speed scales: Dragged < Solo < Coop < None.
	TestEqual(TEXT("Solo is slower than None"), HollowCarryPolicy::Slower(EClaim::None, EClaim::Solo, Tuning), EClaim::Solo);
	TestEqual(TEXT("Solo is slower than Coop"), HollowCarryPolicy::Slower(EClaim::Coop, EClaim::Solo, Tuning), EClaim::Solo);
	TestEqual(TEXT("Dragged is the slowest"), HollowCarryPolicy::Slower(EClaim::Solo, EClaim::Dragged, Tuning), EClaim::Dragged);
	TestEqual(TEXT("A tie keeps the first"), HollowCarryPolicy::Slower(EClaim::Solo, EClaim::Solo, Tuning), EClaim::Solo);

	// Pick-up decided by the server (None -> Solo): the client gets the grace to learn it and claim it.
	const FHollowServerCarry Lifted = Server(EClaim::Solo, EClaim::None);
	TestEqual(TEXT("Grace: the old free state still binds"), Effective(EClaim::None, Lifted, 0.0), EClaim::None);
	TestEqual(TEXT("Grace: just before its end"), Effective(EClaim::None, Lifted, Grace - 0.01), EClaim::None);
	TestEqual(TEXT("The grace ends inclusively: the server state binds"), Effective(EClaim::None, Lifted, Grace), EClaim::Solo);
	TestEqual(TEXT("A client that never claims is slowed after the grace"), Effective(EClaim::None, Lifted, Long), EClaim::Solo);
	TestEqual(TEXT("A predicted pick-up claims Solo before the server knows"), Effective(EClaim::Solo, Server(EClaim::None, EClaim::None), Long), EClaim::Solo);
	TestEqual(TEXT("The claim and the server agree"), Effective(EClaim::Solo, Lifted, 0.0), EClaim::Solo);
	TestEqual(TEXT("A negative age (clock skew) is inside the grace"), Effective(EClaim::None, Lifted, -1.0), EClaim::None);

	// Drop (Solo -> None): a gentler server state binds at once, and the slower claim until the client learns it is fine.
	const FHollowServerCarry Dropped = Server(EClaim::None, EClaim::Solo);
	TestEqual(TEXT("After a drop the client still claims Solo: accepted"), Effective(EClaim::Solo, Dropped, 0.0), EClaim::Solo);
	TestEqual(TEXT("A gentler state needs no grace"), Effective(EClaim::None, Dropped, 0.0), EClaim::None);

	// Mode changes (S3-06): the slower of claim and floor.
	TestEqual(TEXT("Claiming Coop while the server says Solo (after the grace) moves Solo"),
		Effective(EClaim::Coop, Server(EClaim::Solo, EClaim::Coop), Long), EClaim::Solo);
	TestEqual(TEXT("Solo -> Coop is gentler: Coop at once"), Effective(EClaim::Coop, Server(EClaim::Coop, EClaim::Solo), 0.0), EClaim::Coop);
	TestEqual(TEXT("A slower claim than the server's is always accepted"), Effective(EClaim::Dragged, Server(EClaim::Coop, EClaim::Coop), Long), EClaim::Dragged);
	TestEqual(TEXT("Coop -> Dragged in the grace keeps Coop for a fast claim"), Effective(EClaim::None, Server(EClaim::Dragged, EClaim::Coop), 0.0), EClaim::Coop);

	// The floor alone.
	TestEqual(TEXT("Floor in the grace"), HollowCarryPolicy::ServerFloor(Lifted, 0.0, Grace, Tuning), EClaim::None);
	TestEqual(TEXT("Floor after the grace"), HollowCarryPolicy::ServerFloor(Lifted, Long, Grace, Tuning), EClaim::Solo);
	TestEqual(TEXT("No grace configured: the server state binds at once"), HollowCarryPolicy::ServerFloor(Lifted, 0.0, 0.f, Tuning), EClaim::Solo);
	TestEqual(TEXT("None tunes nothing"), Tuning.GetSpeedScale(EClaim::None), 1.f);
	TestNull(TEXT("None has no carry tuning"), Tuning.Find(EClaim::None));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarryDroneClaimTest, "Hollow.Physics.Carry.DroneClaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarryDroneClaimTest::RunTest(const FString& Parameters)
{
	// DRONE-01 (ADR-0054): the drone carry has its own claim, packed into the third carry bit of the saved moves, its own
	// tuning, and hands over (faster) and back (slower) without a correction.
	using namespace HollowCarryTests;
	using EClaim = EHollowCarryClaim;
	TestEqual(TEXT("Drone mode claims Drone"), HollowCarry::ClaimFor(EHollowCarryMode::Drone), EClaim::Drone);
	TestEqual(TEXT("Solo mode still claims Solo"), HollowCarry::ClaimFor(EHollowCarryMode::Solo), EClaim::Solo);
	TestEqual(TEXT("Coop mode claims Coop"), HollowCarry::ClaimFor(EHollowCarryMode::Coop), EClaim::Coop);
	TestEqual(TEXT("A winched object has no carrier"), HollowCarry::ClaimFor(EHollowCarryMode::Winched), EClaim::None);

	// Every claim survives the compressed flags, inside the carry bits only (sprint, jump and crouch keep theirs).
	const uint8 Others = FSavedMove_Character::FLAG_JumpPressed | FSavedMove_Character::FLAG_WantsToCrouch | HollowMoveFlags::Sprint;
	TestEqual(TEXT("Carry bits overlap no other flag"), static_cast<uint8>(HollowMoveFlags::CarryMask & Others), static_cast<uint8>(0));
	TestEqual(TEXT("Three carry bits: Custom 1 to 3"), HollowMoveFlags::CarryMask,
		static_cast<uint8>(FSavedMove_Character::FLAG_Custom_1 | FSavedMove_Character::FLAG_Custom_2 | FSavedMove_Character::FLAG_Custom_3));
	for (uint8 Value = 0; Value <= static_cast<uint8>(EClaim::Drone); ++Value)
	{
		const EClaim Claim = static_cast<EClaim>(Value);
		const uint8 Flags = HollowMoveFlags::EncodeCarry(Claim);
		TestEqual(FString::Printf(TEXT("Claim %d stays in the carry bits"), Value), static_cast<uint8>(Flags & ~HollowMoveFlags::CarryMask), static_cast<uint8>(0));
		TestEqual(FString::Printf(TEXT("Claim %d round trip"), Value), HollowMoveFlags::DecodeCarry(Flags | Others), Claim);
	}
	TestEqual(TEXT("Drone uses the third bit"), static_cast<uint8>(HollowMoveFlags::EncodeCarry(EClaim::Drone) & FSavedMove_Character::FLAG_Custom_3),
		static_cast<uint8>(FSavedMove_Character::FLAG_Custom_3));

	const FHollowCarryMovementTuning Tuning = MakeTuning();
	TestTrue(TEXT("Drone has its own tuning"), Tuning.Find(EClaim::Drone) == &Tuning.Drone);
	TestEqual(TEXT("Drone pace"), Tuning.GetSpeedScale(EClaim::Drone), DroneScale);
	TestEqual(TEXT("Carrying alone is slower than the drone"), HollowCarryPolicy::Slower(EClaim::Drone, EClaim::Solo, Tuning), EClaim::Solo);

	// Handing over (Solo -> Drone, faster): the client still claiming Solo is slower than allowed, accepted as it is.
	const FHollowServerCarry HandedOver = Server(EClaim::Drone, EClaim::Solo);
	TestEqual(TEXT("Hand-over: no grace needed"), HollowCarryPolicy::ServerFloor(HandedOver, 0.0, Grace, Tuning), EClaim::Drone);
	TestEqual(TEXT("Hand-over: the late Solo claim stands"), HollowCarryPolicy::Effective(EClaim::Solo, HandedOver, 0.0, Grace, Tuning), EClaim::Solo);
	TestEqual(TEXT("Hand-over: then the Drone pace"), HollowCarryPolicy::Effective(EClaim::Drone, HandedOver, Long, Grace, Tuning), EClaim::Drone);
	// Taking back (Drone -> Solo, slower): the late Drone claim is accepted only within the grace.
	const FHollowServerCarry TakenBack = Server(EClaim::Solo, EClaim::Drone);
	TestEqual(TEXT("Take-back: the Drone claim within the grace"), HollowCarryPolicy::Effective(EClaim::Drone, TakenBack, 0.0, Grace, Tuning), EClaim::Drone);
	TestEqual(TEXT("Take-back: Solo after the grace"), HollowCarryPolicy::Effective(EClaim::Drone, TakenBack, Long, Grace, Tuning), EClaim::Solo);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
