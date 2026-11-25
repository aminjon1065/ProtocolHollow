#include "Tests/HollowGASTestRig.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/HollowWeaponSet.h"
#include "Weapons/HollowWeaponMath.h"

namespace HollowWeaponMathTests
{
	// Test data.
	constexpr int32 Pellets = 8;
	constexpr float Spread = 5.f;
	constexpr int32 Seed = 4242;
	const FVector Aim = FVector(1.0, 0.25, -0.1).GetSafeNormal();

	/** A shot that passes every check: 4 m straight ahead onto a 1 m box. */
	FHollowShotCheck GoodShot()
	{
		FHollowShotCheck Check;
		Check.ServerOrigin = FVector::ZeroVector;
		Check.ServerAim = FVector::ForwardVector;
		Check.ClaimedOrigin = FVector(10.0, 0.0, 0.0);
		Check.ClaimedDirection = FVector::ForwardVector;
		Check.ImpactPoint = FVector(400.0, 0.0, 0.0);
		Check.TargetBounds = FBox(FVector(400.0, -50.0, -50.0), FVector(500.0, 50.0, 50.0));
		Check.Range = 1000.f;
		Check.SpreadDegrees = Spread;
		Check.AimToleranceDegrees = 4.f;
		Check.OriginToleranceCm = 80.f;
		Check.BoundsToleranceCm = 40.f;
		return Check;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWeaponMathTest, "Hollow.GAS.Weapons.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWeaponMathTest::RunTest(const FString& Parameters)
{
	using namespace HollowWeaponMathTests;
	using namespace HollowWeaponMath;

	// The pellets: the same for the same serial (the owner's hits, everyone's tracers), inside the cone, apart.
	TArray<FVector> First;
	TArray<FVector> Again;
	TArray<FVector> Other;
	PelletDirections(Aim, Pellets, Spread, Seed, First);
	PelletDirections(Aim, Pellets, Spread, Seed, Again);
	PelletDirections(Aim, Pellets, Spread, Seed + 1, Other);
	TestEqual(TEXT("One direction per pellet"), First.Num(), Pellets);
	TestTrue(TEXT("The same serial: the same pellets"), First == Again);
	TestFalse(TEXT("Another serial: other pellets"), First == Other);
	const float Cos = FMath::Cos(FMath::DegreesToRadians(Spread)) - KINDA_SMALL_NUMBER;
	TestTrue(TEXT("Every pellet inside the cone"), First.FindByPredicate([Cos](const FVector& D) { return FVector::DotProduct(D, HollowWeaponMathTests::Aim) < Cos; }) == nullptr);
	TestTrue(TEXT("Every pellet a unit direction"), First.FindByPredicate([](const FVector& D) { return !D.IsNormalized(); }) == nullptr);
	TArray<FVector> Straight;
	PelletDirections(Aim, 1, 0.f, Seed, Straight);
	TestTrue(TEXT("No spread: straight ahead"), Straight.Num() == 1 && Straight[0].Equals(HollowWeaponMathTests::Aim, 1e-5));

	// Falloff: full up to the start, the floor from the end, a line between.
	TestEqual(TEXT("Full damage up close"), DamageAtDistance(20.f, 500.f, 600.f, 1800.f, 0.35f), 20.f);
	TestEqual(TEXT("Half way down the ramp"), DamageAtDistance(20.f, 1200.f, 600.f, 1800.f, 0.4f), 20.f * 0.7f);
	TestEqual(TEXT("The floor at the end and beyond"), DamageAtDistance(20.f, 5000.f, 600.f, 1800.f, 0.35f), 20.f * 0.35f);
	TestEqual(TEXT("No falloff configured: full damage"), DamageAtDistance(20.f, 5000.f, 0.f, 0.f, 1.f), 20.f);

	// The server's check, in its order.
	TestEqual(TEXT("A good hit is taken"), CheckHit(GoodShot()), EHollowShotVerdict::Accepted);
	FHollowShotCheck Check = GoodShot();
	Check.ClaimedOrigin = FVector(0.0, 200.0, 0.0);
	TestEqual(TEXT("Eyes elsewhere than the server sees them"), CheckHit(Check), EHollowShotVerdict::OriginTooFar);
	Check = GoodShot();
	Check.ClaimedDirection = FVector(1.0, 1.0, 0.0).GetSafeNormal();
	TestEqual(TEXT("A pellet outside the cone plus the tolerance"), CheckHit(Check), EHollowShotVerdict::OutsideCone);
	Check = GoodShot();
	Check.ClaimedDirection = FRotator(0.0, Spread + 3.0, 0.0).Vector();
	Check.ImpactPoint = Check.ClaimedOrigin + Check.ClaimedDirection * 400.0;
	Check.TargetBounds = FBox(Check.ImpactPoint - FVector(50.0), Check.ImpactPoint + FVector(50.0));
	TestEqual(TEXT("...but within the aim tolerance it is taken"), CheckHit(Check), EHollowShotVerdict::Accepted);
	Check = GoodShot();
	Check.Range = 200.f;
	TestEqual(TEXT("Farther than the weapon reaches"), CheckHit(Check), EHollowShotVerdict::OutOfRange);
	Check = GoodShot();
	Check.TargetBounds = FBox(FVector(400.0, 300.0, -50.0), FVector(500.0, 400.0, 50.0));
	TestEqual(TEXT("An impact nowhere near the target"), CheckHit(Check), EHollowShotVerdict::NotOnTarget);
	Check.TargetVelocity = FVector(0.0, -2000.0, 0.0);
	Check.LatencySeconds = 0.15f;
	TestEqual(TEXT("...where it was a moment ago, it moving fast: taken"), CheckHit(Check), EHollowShotVerdict::Accepted);
	Check.LatencySeconds = 5.f;
	Check.TargetVelocity = FVector(0.0, 20.0, 0.0);
	TestEqual(TEXT("...a long latency rewinds no farther than the cap"), CheckHit(Check), EHollowShotVerdict::NotOnTarget);
	Check = GoodShot();
	Check.bLineOfSight = false;
	TestEqual(TEXT("A wall between the server's eyes and the impact"), CheckHit(Check), EHollowShotVerdict::Blocked);

	// A reload: what the magazine lacks, at most what is carried.
	TestEqual(TEXT("Empty magazine, plenty carried"), RoundsToLoad(0, 12, 40), 12);
	TestEqual(TEXT("Half full"), RoundsToLoad(5, 12, 40), 7);
	TestEqual(TEXT("Fewer carried than it lacks"), RoundsToLoad(0, 12, 3), 3);
	TestEqual(TEXT("Full: nothing"), RoundsToLoad(12, 12, 40), 0);
	TestEqual(TEXT("Nothing carried: nothing"), RoundsToLoad(0, 12, 0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowWeaponSetTest, "Hollow.GAS.Weapons.Magazine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowWeaponSetTest::RunTest(const FString& Parameters)
{
	// GP-01: the magazine stays in 0..its size, whatever the effects add or take.
	FHollowGASTestRig Rig;
	if (!Rig.Create<UHollowWeaponSet>(*this))
	{
		return false;
	}
	const FGameplayAttribute Magazine = UHollowWeaponSet::GetMagazineAttribute();
	const FGameplayAttribute Size = UHollowWeaponSet::GetMaxMagazineAttribute();
	Rig.Apply(Size, EGameplayModOp::Override, 12.f);
	Rig.Apply(Magazine, EGameplayModOp::Override, 12.f);
	TestEqual(TEXT("A full magazine"), Rig.Value(Magazine), 12.f);
	Rig.Apply(Magazine, EGameplayModOp::Additive, -1.f);
	TestEqual(TEXT("A shot takes a round"), Rig.Value(Magazine), 11.f);
	Rig.Apply(Magazine, EGameplayModOp::Additive, 20.f);
	TestEqual(TEXT("A reload never overfills"), Rig.Value(Magazine), 12.f);
	Rig.Apply(Magazine, EGameplayModOp::Additive, -40.f);
	TestEqual(TEXT("...and the magazine never goes below empty"), Rig.Value(Magazine), 0.f);
	Rig.Apply(Magazine, EGameplayModOp::Override, 12.f);
	Rig.Apply(Size, EGameplayModOp::Override, 4.f);
	TestEqual(TEXT("A smaller magazine pulls the rounds down with it"), Rig.Value(Magazine), 4.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
