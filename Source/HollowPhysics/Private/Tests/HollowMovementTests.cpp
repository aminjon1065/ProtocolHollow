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

#endif // WITH_DEV_AUTOMATION_TESTS
