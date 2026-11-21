#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Movement/HollowMovementMath.h"
#include "Movement/HollowMovementTuning.h"

namespace HollowMovementTests
{
	// Test data only: real values live in DA_MovementTuning_* assets.
	constexpr float WalkSpeed = 400.f;
	constexpr float CrouchSpeed = 200.f;
	constexpr float SprintMultiplier = 1.5f;
	constexpr float SprintMaxAngle = 45.f;

	FHollowGroundSpeedTuning MakeTuning(const float MaxAngle = SprintMaxAngle, const float Multiplier = SprintMultiplier)
	{
		FHollowGroundSpeedTuning Tuning;
		Tuning.WalkSpeed = WalkSpeed;
		Tuning.CrouchSpeed = CrouchSpeed;
		Tuning.SprintSpeedMultiplier = Multiplier;
		Tuning.SprintMaxAngle = MaxAngle;
		return Tuning;
	}

	FVector DirectionAtYaw(const float YawDegrees)
	{
		return FRotator(0.f, YawDegrees, 0.f).Vector();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowMovementSpeedModelTest, "Hollow.Physics.Movement.SpeedModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowMovementSpeedModelTest::RunTest(const FString& Parameters)
{
	using namespace HollowMovementTests;
	const FHollowGroundSpeedTuning Tuning = MakeTuning();
	const FVector Facing = FVector::ForwardVector;

	TestEqual(TEXT("Sprinting forward applies the multiplier"),
		HollowMovementMath::GetSpeedMultiplier(Tuning, true, false, Facing, Facing), SprintMultiplier);
	TestEqual(TEXT("No sprint request walks"),
		HollowMovementMath::GetSpeedMultiplier(Tuning, false, false, Facing, Facing), 1.f);
	TestFalse(TEXT("Crouching cancels sprint"), HollowMovementMath::CanSprint(Tuning, true, true, Facing, Facing));
	TestFalse(TEXT("Backwards is not a sprint"), HollowMovementMath::CanSprint(Tuning, true, false, -Facing, Facing));
	TestFalse(TEXT("Strafing beyond the angle is not a sprint"), HollowMovementMath::CanSprint(Tuning, true, false, DirectionAtYaw(90.f), Facing));
	TestTrue(TEXT("Diagonal within the angle sprints"), HollowMovementMath::CanSprint(Tuning, true, false, DirectionAtYaw(30.f), Facing));
	TestTrue(TEXT("The angle limit is inclusive"), HollowMovementMath::CanSprint(Tuning, true, false, DirectionAtYaw(SprintMaxAngle), Facing));
	TestFalse(TEXT("No movement input is not a sprint"), HollowMovementMath::CanSprint(Tuning, true, false, FVector::ZeroVector, Facing));
	TestTrue(TEXT("Vertical components are ignored"),
		HollowMovementMath::CanSprint(Tuning, true, false, FVector(1.f, 0.f, 5.f), FVector(1.f, 0.f, -3.f)));
	TestFalse(TEXT("A multiplier of 1 disables sprint"), HollowMovementMath::CanSprint(MakeTuning(SprintMaxAngle, 1.f), true, false, Facing, Facing));

	const FHollowGroundSpeedTuning WideAngle = MakeTuning(120.f);   // out of range: clamped to 90 degrees
	TestTrue(TEXT("Clamped angle still allows strafing at 90 degrees"), HollowMovementMath::CanSprint(WideAngle, true, false, DirectionAtYaw(90.f), Facing));
	TestFalse(TEXT("Clamped angle never allows backwards"), HollowMovementMath::CanSprint(WideAngle, true, false, -Facing, Facing));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowMovementMoveStepsTest, "Hollow.Physics.Movement.MoveSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowMovementMoveStepsTest::RunTest(const FString& Parameters)
{
	// ADR-0028 (the cause of ADR-0015's first-step correction): a client frame longer than MaxMoveDeltaTime goes out as
	// several whole moves, so the client simulates the time the server derives from the timestamps.
	constexpr float MaxMove = 0.125f;     // GameNetworkManager MaxMoveDeltaTime (test data)
	constexpr float Frame30 = 1.f / 30.f;
	constexpr float Hitch = 0.18f;        // a long frame seen in the S3-06 smoke
	constexpr float Freeze = 5.f;
	TestEqual(TEXT("An ordinary frame: one move"), HollowMovementMath::MoveSteps(Frame30, MaxMove), 1);
	TestEqual(TEXT("Exactly the limit: one move"), HollowMovementMath::MoveSteps(MaxMove, MaxMove), 1);
	const int32 Steps = HollowMovementMath::MoveSteps(Hitch, MaxMove);
	TestEqual(TEXT("A 0.18 s frame: two moves"), Steps, 2);
	TestTrue(TEXT("...each within the limit, together the whole frame"), Hitch / Steps <= MaxMove && FMath::IsNearlyEqual(Steps * (Hitch / Steps), Hitch));
	TestEqual(TEXT("A freeze is capped (the engine clamps the rest)"), HollowMovementMath::MoveSteps(Freeze, MaxMove), HollowMovementMath::MaxMovesPerFrame);
	TestEqual(TEXT("No limit known: one move"), HollowMovementMath::MoveSteps(Hitch, 0.f), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
