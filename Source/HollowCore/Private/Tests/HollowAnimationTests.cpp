#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/HollowCharacterAnimInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAnimationLocomotionTest, "Hollow.Core.Animation.Locomotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAnimationLocomotionTest::RunTest(const FString& Parameters)
{
	// CHAR-01 (ADR-0055): what a character's animation blueprint blends by, from its velocity, facing and aim.
	constexpr float Tolerance = 0.01f;
	const FRotator North(0.0, 0.0, 0.0);
	const FRotator East(0.0, 90.0, 0.0);

	const FHollowLocomotion Forward = FHollowLocomotion::From(FVector(400.0, 0.0, 0.0), North, North, false);
	TestEqual(TEXT("Ground speed"), Forward.Speed, 400.f, Tolerance);
	TestEqual(TEXT("...straight ahead"), Forward.Direction, 0.f, Tolerance);

	const FHollowLocomotion Falling = FHollowLocomotion::From(FVector(0.0, 300.0, -900.0), North, North, true);
	TestEqual(TEXT("A fall's speed is only across the floor"), Falling.Speed, 300.f, Tolerance);
	TestEqual(TEXT("...to its right"), Falling.Direction, 90.f, Tolerance);
	TestTrue(TEXT("...falling"), Falling.bFalling);

	TestEqual(TEXT("Facing east, moving north: to its left"), FHollowLocomotion::From(FVector(200.0, 0.0, 0.0), East, East, false).Direction, -90.f, Tolerance);
	TestEqual(TEXT("Backwards: half a turn"), FMath::Abs(FHollowLocomotion::From(FVector(-200.0, 0.0, 0.0), North, North, false).Direction), 180.f, Tolerance);

	const FHollowLocomotion Standing = FHollowLocomotion::From(FVector(1.0, 1.0, 0.0), North, North, false);
	TestEqual(TEXT("Below the standing speed: no direction (a turn in place)"), Standing.Direction, 0.f, Tolerance);

	TestEqual(TEXT("Looking down 10 degrees (a view pitch of 350)"), FHollowLocomotion::From(FVector::ZeroVector, North, FRotator(350.0, 0.0, 0.0), false).AimPitch,
		-10.f, Tolerance);
	TestEqual(TEXT("...up 30"), FHollowLocomotion::From(FVector::ZeroVector, North, FRotator(30.0, 0.0, 0.0), false).AimPitch, 30.f, Tolerance);
	TestEqual(TEXT("...relative to the body"), FHollowLocomotion::From(FVector::ZeroVector, FRotator(10.0, 0.0, 0.0), FRotator(30.0, 0.0, 0.0), false).AimPitch,
		20.f, Tolerance);
	TestTrue(TEXT("...never past straight up"), FHollowLocomotion::From(FVector::ZeroVector, FRotator(-80.0, 0.0, 0.0), FRotator(80.0, 0.0, 0.0), false).AimPitch
		<= FHollowLocomotion::MaxAimPitch);
	return true;
}

#endif
