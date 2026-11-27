#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/HollowAnimationAuthoring.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "UObject/Package.h"

namespace HollowAnimationAuthoringTests
{
	// Content the authoring works on (CHAR-01, ADR-0055): the Iron Pilgrim's Paragon Riktor.
	const TCHAR* const Riktor = TEXT("/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor/Meshes/Riktor.Riktor");
	const TCHAR* const Idle = TEXT("/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor/Animations/Idle_Combat.Idle_Combat");
	const TCHAR* const Jog = TEXT("/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor/Animations/Jog_Fwd.Jog_Fwd");
	const TCHAR* const StunStart = TEXT("/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor/Animations/Stun_Start.Stun_Start");
	const TCHAR* const StunLoop = TEXT("/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor/Animations/Stun_Loop.Stun_Loop");
	const FName HiddenChain(TEXT("M06_chain_r_long"));   // fab_manifest.json hidden_slots
	/** Test data. */
	constexpr float Chase = 330.f;
	constexpr float JogRate = 0.7f;
	constexpr float LoopSeconds = 1.f;
	constexpr float Tolerance = 0.01f;

	FHollowBlendAxis Axis(const TCHAR* Name, const float Max)
	{
		FHollowBlendAxis Result;
		Result.Name = Name;
		Result.Max = Max;
		return Result;
	}

	FHollowBlendSampleSpec Sample(UAnimSequence* Animation, const float X, const float Rate)
	{
		FHollowBlendSampleSpec Result;
		Result.Animation = Animation;
		Result.Value = FVector(X, 0.0, 0.0);
		Result.RateScale = Rate;
		return Result;
	}

	FHollowMontageSection Section(const TCHAR* Name, const float Time, const TCHAR* Next)
	{
		FHollowMontageSection Result;
		Result.Name = Name;
		Result.Time = Time;
		Result.Next = Next;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAnimationAuthoringTest, "Hollow.Editor.AnimationAuthoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAnimationAuthoringTest::RunTest(const FString& Parameters)
{
	// CHAR-01 (ADR-0055): what create_character_assets.py builds through UHollowAnimationAuthoring, on transient copies:
	// a blend space's axes and samples with their rate, a montage of two stretches with a looping section, a skeleton's
	// socket and slot; nonsense is refused and said; a mesh's hidden slots are counted off.
	using namespace HollowAnimationAuthoringTests;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Riktor);
	UAnimSequence* IdleAnim = LoadObject<UAnimSequence>(nullptr, Idle);
	UAnimSequence* JogAnim = LoadObject<UAnimSequence>(nullptr, Jog);
	UAnimSequence* StartAnim = LoadObject<UAnimSequence>(nullptr, StunStart);
	UAnimSequence* LoopAnim = LoadObject<UAnimSequence>(nullptr, StunLoop);
	if (!TestNotNull(TEXT("Riktor"), Mesh) || !TestTrue(TEXT("...and its animations"), IdleAnim && JogAnim && StartAnim && LoopAnim))
	{
		return false;
	}
	USkeleton* Skeleton = Mesh->GetSkeleton();

	UBlendSpace1D* Space = NewObject<UBlendSpace1D>(GetTransientPackage());
	Space->SetSkeleton(Skeleton);
	TestTrue(TEXT("A speed axis, standing and a slow jog"), UHollowAnimationAuthoring::SetBlendSpace(Space, {Axis(TEXT("Speed"), Chase)},
		{Sample(IdleAnim, 0.f, 1.f), Sample(JogAnim, Chase, JogRate)}));
	TestEqual(TEXT("...two samples"), Space->GetNumberOfBlendSamples(), 2);
	TestTrue(TEXT("...the jog at the chase speed, slower"), Space->GetNumberOfBlendSamples() == 2 && Space->GetBlendSample(1).Animation == JogAnim
		&& FMath::IsNearlyEqual(Space->GetBlendSample(1).SampleValue.X, Chase, Tolerance) && FMath::IsNearlyEqual(Space->GetBlendSample(1).RateScale, JogRate, Tolerance));
	TestTrue(TEXT("...again: the same, not twice"), UHollowAnimationAuthoring::SetBlendSpace(Space, {Axis(TEXT("Speed"), Chase)},
		{Sample(IdleAnim, 0.f, 1.f), Sample(JogAnim, Chase, JogRate)}) && Space->GetNumberOfBlendSamples() == 2);
	AddExpectedMessage(TEXT("needs 1 axis"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	TestFalse(TEXT("Two axes on a 1D blend space: refused"), UHollowAnimationAuthoring::SetBlendSpace(Space, {Axis(TEXT("A"), 1.f), Axis(TEXT("B"), 1.f)}, {}));

	UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
	Montage->SetSkeleton(Skeleton);
	FHollowMontageSegment Start;
	Start.Animation = StartAnim;
	FHollowMontageSegment Loop;
	Loop.Animation = LoopAnim;
	Loop.EndTime = LoopSeconds;
	const float LoopAt = StartAnim->GetPlayLength();
	TestTrue(TEXT("A stun: its start, then one second of its loop, looping"), UHollowAnimationAuthoring::SetMontage(Montage, TEXT("DefaultSlot"), {Start, Loop},
		{Section(TEXT("Start"), 0.f, TEXT("Loop")), Section(TEXT("Loop"), LoopAt, TEXT("Loop"))}, 0.2f, 0.3f));
	const int32 LoopIndex = Montage->GetSectionIndex(TEXT("Loop"));
	TestTrue(TEXT("...one slot, two stretches, as long as both"), Montage->SlotAnimTracks.Num() == 1 && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() == 2
		&& FMath::IsNearlyEqual(Montage->GetPlayLength(), LoopAt + LoopSeconds, Tolerance));
	TestTrue(TEXT("...the loop goes round"), LoopIndex != INDEX_NONE && Montage->CompositeSections[LoopIndex].NextSectionName == TEXT("Loop")
		&& FMath::IsNearlyEqual(Montage->CompositeSections[LoopIndex].GetTime(), LoopAt, Tolerance));
	AddExpectedMessage(TEXT("cannot have section"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	TestFalse(TEXT("A section past its end: refused"), UHollowAnimationAuthoring::SetMontage(Montage, TEXT("DefaultSlot"), {Start},
		{Section(TEXT("Late"), LoopAt + LoopSeconds * 10.f, TEXT(""))}, 0.f, 0.f));

	USkeleton* Copy = DuplicateObject(Skeleton, GetTransientPackage());
	const FTransform Grip(FRotator(0.0, 90.0, 0.0), FVector(2.0, 3.0, 4.0));
	TestTrue(TEXT("A socket on the right hand"), UHollowAnimationAuthoring::SetSkeletonSocket(Copy, TEXT("HollowTestSocket"), TEXT("hand_r"), Grip)
		&& Copy->FindSocket(TEXT("HollowTestSocket")) && Copy->FindSocket(TEXT("HollowTestSocket"))->RelativeLocation.Equals(Grip.GetLocation(), Tolerance));
	AddExpectedMessage(TEXT("has no bone"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	TestFalse(TEXT("...on a bone it has not: refused"), UHollowAnimationAuthoring::SetSkeletonSocket(Copy, TEXT("HollowTestSocket"), TEXT("no_such_bone"), Grip));
	TestTrue(TEXT("A slot it learns"), UHollowAnimationAuthoring::EnsureSkeletonSlot(Copy, TEXT("HollowTestSlot")) && Copy->ContainsSlotName(TEXT("HollowTestSlot")));

	const FIntPoint Hidden = UHollowAnimationAuthoring::CountDisabledSections(Mesh, {HiddenChain});
	TestTrue(FString::Printf(TEXT("Riktor's hidden chain: %d of %d sections off"), Hidden.X, Hidden.Y), Hidden.Y > 0 && Hidden.X == Hidden.Y);
	return true;
}

#endif
