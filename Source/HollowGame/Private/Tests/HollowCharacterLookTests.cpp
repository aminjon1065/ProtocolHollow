#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/HollowCharacterAnimInstance.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Tools/HollowToolMountComponent.h"
#include "UObject/UnrealType.h"

namespace HollowCharacterLookTests
{
	// Content under test (create_character_assets.py, CHAR-01, ADR-0055).
	const TCHAR* const ExoFrameClass = TEXT("/Game/Hollow/Characters/ExoFrame/BP_ExoFrame.BP_ExoFrame_C");
	const TCHAR* const FabCharacters = TEXT("/Game/Fab/Paragon");
	const FName DefaultSlot(TEXT("DefaultSlot"));
	const FName ToolSocket(TEXT("ToolMount"));
	const FName Head(TEXT("head"));   // the bone the capsule's top is measured against

	/** Each character, its capsule as its own scripts make it (create_exoframe/pilgrim/stalker_assets.py: never changed here). */
	struct FCharacter
	{
		const TCHAR* Class;
		float Radius;
		float HalfHeight;
	};
	const FCharacter Characters[] = {
		{ExoFrameClass, 34.f, 96.f},
		{TEXT("/Game/Hollow/Characters/Enemies/IronPilgrim/BP_IronPilgrim.BP_IronPilgrim_C"), 80.f, 140.f},
		{TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/BP_EchoStalker.BP_EchoStalker_C"), 45.f, 110.f},
	};
	/** The head bone sits in the capsule's upper part: below its top, above this share of its height. */
	constexpr float HeadLowestShare = 0.8f;
	constexpr float Tolerance = 0.01f;

	/** Whether an animation blueprint's graph has a Slot node for SlotName (FAnimNode_Slot, read by reflection). */
	bool HasSlot(const UClass* AnimClass, const FName SlotName)
	{
		const UAnimBlueprintGeneratedClass* Generated = Cast<UAnimBlueprintGeneratedClass>(AnimClass);
		const UObject* Defaults = AnimClass ? AnimClass->GetDefaultObject() : nullptr;
		if (!Generated || !Defaults)
		{
			return false;
		}
		for (const FStructProperty* Node : Generated->GetAnimNodeProperties())
		{
			const FNameProperty* Name = Node->Struct->GetName() == TEXT("AnimNode_Slot") ? CastField<FNameProperty>(Node->Struct->FindPropertyByName(TEXT("SlotName"))) : nullptr;
			if (Name && Name->GetPropertyValue_InContainer(Node->ContainerPtrToValuePtr<void>(Defaults)) == SlotName)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCharacterLookTest, "Hollow.Game.Characters.Look",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCharacterLookTest::RunTest(const FString& Parameters)
{
	// CHAR-01 (ADR-0055): each character wears its own Paragon mesh in its unchanged capsule, feet on the capsule's
	// bottom and head in its top; its animation blueprint is a UHollowCharacterAnimInstance whose graph plays montages in
	// the DefaultSlot; an unseen enemy skips its pose. The ExoFrame's camera sits at its body's head; the body holds the
	// tool in its ToolMount socket; it has no first-person arms (AK-4).
	using namespace HollowCharacterLookTests;
	for (const FCharacter& Character : Characters)
	{
		const UClass* Class = LoadClass<ACharacter>(nullptr, Character.Class);
		const ACharacter* Defaults = Class ? Class->GetDefaultObject<ACharacter>() : nullptr;
		const USkeletalMeshComponent* Body = Defaults ? Defaults->GetMesh() : nullptr;
		const USkeletalMesh* Mesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
		if (!TestNotNull(FString::Printf(TEXT("%s: a body mesh"), Character.Class), Mesh))
		{
			continue;
		}
		const FString Name = Class->GetName();
		const UCapsuleComponent* Capsule = Defaults->GetCapsuleComponent();
		TestTrue(Name + TEXT(": its capsule as before"), FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(), Character.Radius, Tolerance)
			&& FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleHalfHeight(), Character.HalfHeight, Tolerance));
		TestTrue(Name + TEXT(": a Paragon character of Content/Fab"), Mesh->GetPathName().StartsWith(FabCharacters));
		TestTrue(Name + TEXT(": feet on the capsule's bottom"), FMath::IsNearlyEqual(Body->GetRelativeLocation().Z, -Character.HalfHeight, Tolerance));
		const int32 HeadIndex = Mesh->GetRefSkeleton().FindBoneIndex(Head);
		const float HeadHeight = HeadIndex == INDEX_NONE ? 0.f
			: static_cast<float>(FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), HeadIndex).GetLocation().Z * Body->GetRelativeScale3D().Z);
		TestTrue(FString::Printf(TEXT("%s: the head %.0f cm up, in the capsule's top (%.0f)"), *Name, HeadHeight, 2.f * Character.HalfHeight),
			HeadHeight <= 2.f * Character.HalfHeight && HeadHeight >= HeadLowestShare * 2.f * Character.HalfHeight);
		const UClass* AnimClass = Body->AnimClass.Get();
		TestTrue(Name + TEXT(": a UHollowCharacterAnimInstance"), AnimClass && AnimClass->IsChildOf(UHollowCharacterAnimInstance::StaticClass()));
		TestTrue(Name + TEXT(": ...its montages play in the DefaultSlot"), HasSlot(AnimClass, DefaultSlot));
		if (FCString::Strcmp(Character.Class, ExoFrameClass) != 0)
		{
			TestTrue(Name + TEXT(": unseen, it skips its pose"), Body->VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered);
		}
	}

	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	const TSubclassOf<AHollowExoFrameCharacter> ExoFrame = LoadClass<AHollowExoFrameCharacter>(nullptr, ExoFrameClass);
	AHollowExoFrameCharacter* Character = World && ExoFrame ? World->SpawnActor<AHollowExoFrameCharacter>(ExoFrame, FTransform::Identity) : nullptr;
	if (!TestNotNull(TEXT("BP_ExoFrame spawns"), Character))
	{
		return false;
	}
	TestTrue(TEXT("No first-person arms (Wraith has no first-person poses: AK-4)"), Character->GetFirstPersonMesh()->GetSkeletalMeshAsset() == nullptr);
	TestTrue(TEXT("The tool's socket on the body"), Character->GetMesh()->DoesSocketExist(ToolSocket));
	TestTrue(TEXT("The camera at the body's head"), Character->GetFirstPersonCamera()->GetAttachParent() == Character->GetMesh()
		&& Character->GetFirstPersonCamera()->GetAttachSocketName() == AHollowExoFrameCharacter::CameraSocketName);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
