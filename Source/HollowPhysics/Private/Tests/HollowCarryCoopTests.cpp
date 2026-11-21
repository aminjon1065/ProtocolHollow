#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Carry/HollowCarryCoop.h"

namespace HollowCarryCoopTests
{
	// Test data only: real values live in DA_Carryable_AetherCore and DefaultGame.ini.
	constexpr float Spacing = 140.f;
	constexpr float Slack = 15.f;
	constexpr float PullRate = 4.f;
	constexpr float MaxPull = 150.f;
	constexpr float Break = 100.f;
	constexpr float MaxSpeed = 300.f;
	constexpr double Tolerance = 0.01;
	constexpr double GripHeight = 5.0;
	constexpr float Interval = 0.25f;
	constexpr float SpeedTolerance = 1.3f;
	constexpr float Settle = 0.5f;
	constexpr float Timeout = 2.f;

	FHollowCoopCarryTuning MakeTuning()
	{
		FHollowCoopCarryTuning Tuning;
		Tuning.CarrierSpacing = Spacing;
		Tuning.Slack = Slack;
		Tuning.PullRate = PullRate;
		Tuning.MaxPullSpeed = MaxPull;
		Tuning.BreakDistance = Break;
		return Tuning;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarryTetherTest, "Hollow.Physics.Carry.Tether",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarryTetherTest::RunTest(const FString& Parameters)
{
	// ADR-0028: the partner stands on +X. Within the slack the velocity is the carrier's own; stretched, it never walks
	// away and is pulled back; squeezed, it no longer walks in; never faster than the carry speed; Z untouched.
	using namespace HollowCarryCoopTests;
	const FHollowCoopCarryTuning Tuning = MakeTuning();
	const FVector Self = FVector::ZeroVector;
	auto Partner = [](const double Distance) { return FVector(Distance, 0.0, 0.0); };
	auto Constrain = [&](const FVector& Velocity, const double Distance)
	{
		return HollowCarryTether::Constrain(Velocity, Self, Partner(Distance), Tuning, MaxSpeed);
	};

	const FVector Sideways(0.0, 200.0, -50.0);
	TestEqual(TEXT("In the slack: unchanged"), Constrain(Sideways, Spacing), Sideways, Tolerance);
	TestEqual(TEXT("At the slack's far edge: unchanged"), Constrain(FVector(-100.0, 0.0, 0.0), Spacing + Slack), FVector(-100.0, 0.0, 0.0), Tolerance);

	// Stretched by 10 cm: pull 40 cm/s towards the partner, walking away cancelled, sideways kept.
	const double Stretch = 10.0;
	const FVector Away(-200.0, 100.0, -50.0);
	const FVector Pulled = Constrain(Away, Spacing + Slack + Stretch);
	TestEqual(TEXT("Stretched: walking away turns into the pull"), Pulled.X, PullRate * Stretch, Tolerance);
	TestEqual(TEXT("...sideways kept"), Pulled.Y, 100.0, Tolerance);
	TestEqual(TEXT("...falling kept"), Pulled.Z, -50.0, Tolerance);
	TestEqual(TEXT("Stretched, already walking in faster than the pull: unchanged"), Constrain(FVector(250.0, 0.0, 0.0), Spacing + Slack + Stretch).X, 250.0, Tolerance);
	TestEqual(TEXT("Stretched far: the pull is capped"), Constrain(FVector::ZeroVector, Spacing + Slack + 1000.0).X, static_cast<double>(MaxPull), Tolerance);

	// Squeezed: no further in, away is fine.
	TestEqual(TEXT("Squeezed: walking in is cancelled"), Constrain(FVector(200.0, 50.0, 0.0), Spacing - Slack - 1.0), FVector(0.0, 50.0, 0.0), Tolerance);
	TestEqual(TEXT("Squeezed: walking away is fine"), Constrain(FVector(-200.0, 0.0, 0.0), Spacing - Slack - 1.0).X, -200.0, Tolerance);

	// Speed cap along the ground, and a partner above or below counts along the ground only.
	TestEqual(TEXT("Never faster than the carry speed"), Constrain(FVector(0.0, 500.0, 0.0), Spacing).Size2D(), static_cast<double>(MaxSpeed), Tolerance);
	TestEqual(TEXT("A partner on the stairs above: along the ground"),
		HollowCarryTether::Constrain(Sideways, Self, FVector(Spacing, 0.0, 300.0), Tuning, MaxSpeed), Sideways, Tolerance);
	TestEqual(TEXT("On one spot: no direction, only the cap"), HollowCarryTether::Constrain(FVector(400.0, 0.0, 0.0), Self, Self, Tuning, MaxSpeed).X,
		static_cast<double>(MaxSpeed), Tolerance);

	// Breaking (3D, the limit itself holds).
	TestEqual(TEXT("Max separation"), Tuning.GetMaxSeparation(), Spacing + Break);
	TestFalse(TEXT("At the limit: still carried"), HollowCarryTether::IsBroken(Self, Partner(Spacing + Break), Tuning));
	TestTrue(TEXT("Beyond it: broken"), HollowCarryTether::IsBroken(Self, Partner(Spacing + Break + 1.0), Tuning));
	TestTrue(TEXT("Height counts for breaking"), HollowCarryTether::IsBroken(Self, FVector(Spacing, 0.0, Break + Spacing), Tuning));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarryCoopPoseTest, "Hollow.Physics.Carry.CoopPose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarryCoopPoseTest::RunTest(const FString& Parameters)
{
	// ADR-0028 "virtual stretcher": midway between the two grips, along the ground from the primary to the secondary,
	// whatever way the carriers face; on one spot it keeps the primary's facing.
	using namespace HollowCarryCoopTests;
	const FTransform Primary(FRotator(0.0, 90.0, 0.0), FVector(0.0, 0.0, 100.0));
	const FTransform Secondary(FRotator(0.0, -45.0, 0.0), FVector(Spacing, 0.0, 140.0));
	const FTransform Pose = HollowCarryCoop::Pose(Primary, Secondary, GripHeight);
	TestEqual(TEXT("Midway between the grips"), Pose.GetLocation(), FVector(Spacing * 0.5, 0.0, 120.0 + GripHeight), Tolerance);
	TestEqual(TEXT("X axis from the primary to the secondary"), Pose.GetRotation().GetForwardVector(), FVector::ForwardVector, Tolerance);
	TestEqual(TEXT("Level although they stand at different heights"), Pose.GetRotation().GetUpVector(), FVector::UpVector, Tolerance);
	const FTransform Swapped = HollowCarryCoop::Pose(Secondary, Primary, GripHeight);
	TestEqual(TEXT("Swapped carriers: the same spot"), Swapped.GetLocation(), Pose.GetLocation(), Tolerance);
	TestEqual(TEXT("...facing the other way"), Swapped.GetRotation().GetForwardVector(), -FVector::ForwardVector, Tolerance);
	const FTransform Stacked = HollowCarryCoop::Pose(Primary, FTransform(FVector(0.0, 0.0, 300.0)), GripHeight);
	TestEqual(TEXT("On one spot: the primary's facing"), Stacked.GetRotation().GetForwardVector(), FVector::RightVector, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarryCoopSanityTest, "Hollow.Physics.Carry.CoopSanity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarryCoopSanityTest::RunTest(const FString& Parameters)
{
	// ADR-0028: the server's check of client-authoritative carriers and the clean way back to server authority.
	using namespace HollowCarryCoopTests;
	const double Limit = MaxSpeed * SpeedTolerance * Interval;
	TestFalse(TEXT("Just under the speed limit: passes"), HollowCoopSanity::IsTooFast(FVector::ZeroVector, FVector(Limit - Tolerance, 0.0, 0.0), Interval, MaxSpeed, SpeedTolerance));
	TestTrue(TEXT("Beyond it: too fast"), HollowCoopSanity::IsTooFast(FVector::ZeroVector, FVector(Limit + 1.0, 0.0, 0.0), Interval, MaxSpeed, SpeedTolerance));
	TestFalse(TEXT("Falling is not speeding (along the ground only)"), HollowCoopSanity::IsTooFast(FVector::ZeroVector, FVector(0.0, 0.0, -10.0 * Limit), Interval, MaxSpeed, SpeedTolerance));
	TestTrue(TEXT("Any move in no time is too fast"), HollowCoopSanity::IsTooFast(FVector::ZeroVector, FVector(1.0, 0.0, 0.0), 0.0, MaxSpeed, SpeedTolerance));
	// Jitter: the client moved for longer than the server's interval (its newest move arrived late last time).
	constexpr double Jittered = 0.35;
	TestEqual(TEXT("Moved for the longer of the server's and the client's interval"), HollowCoopSanity::MovedFor(Interval, Jittered), Jittered);
	TestEqual(TEXT("...the server's if the client's is shorter"), HollowCoopSanity::MovedFor(Interval, Interval * 0.5), static_cast<double>(Interval));
	TestEqual(TEXT("...and if the client's timestamps reset"), HollowCoopSanity::MovedFor(Interval, -200.0), static_cast<double>(Interval));
	const double Honest = MaxSpeed * Jittered;   // an honest carrier at full speed over the client's interval
	TestFalse(TEXT("An honest carrier behind jitter passes"), HollowCoopSanity::IsTooFast(FVector::ZeroVector, FVector(Honest, 0.0, 0.0),
		HollowCoopSanity::MovedFor(Interval, Jittered), MaxSpeed, SpeedTolerance));
	TestTrue(TEXT("...it would not by the server's clock alone"), HollowCoopSanity::IsTooFast(FVector::ZeroVector, FVector(Honest, 0.0, 0.0), Interval, MaxSpeed, SpeedTolerance));
	TestTrue(TEXT("Who walked away lets go: the secondary"), HollowCoopSanity::DoesSecondaryLetGo(10.0, 120.0));
	TestFalse(TEXT("...or the primary"), HollowCoopSanity::DoesSecondaryLetGo(120.0, 10.0));
	TestTrue(TEXT("A tie: the object stays with whoever lifted it"), HollowCoopSanity::DoesSecondaryLetGo(50.0, 50.0));

	// The way back: known (a move without the Coop claim) + settled + in sync, or the timeout.
	FHollowCoopReturn Return;
	Return.LeftAt = 10.0;
	TestFalse(TEXT("Not known yet, in sync: still client-authoritative"), Return.ShouldReturn(10.0 + Settle * 3.0, /*bInSync=*/true, Settle, Timeout));
	TestTrue(TEXT("Not known after the timeout: back anyway"), Return.ShouldReturn(10.0 + Timeout, /*bInSync=*/false, Settle, Timeout));
	Return.KnownAt = 10.3;
	TestFalse(TEXT("Known, settling"), Return.ShouldReturn(10.3 + Settle - 0.01, /*bInSync=*/true, Settle, Timeout));
	TestFalse(TEXT("Known, settled, but its move disagrees"), Return.ShouldReturn(10.3 + Settle, /*bInSync=*/false, Settle, Timeout));
	TestTrue(TEXT("Known, settled and agreeing (the settle time is inclusive)"), Return.ShouldReturn(10.3 + Settle, /*bInSync=*/true, Settle, Timeout));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
