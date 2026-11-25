#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Audio/HollowAudioSubsystem.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Feel/HollowFeelCameraShake.h"
#include "Feel/HollowFeelConfig.h"
#include "Feel/HollowFeelSubsystem.h"
#include "Feel/HollowFeelTypes.h"
#include "Feel/HollowViewFeelModifier.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

namespace HollowFeelTests
{
	// Hand-made feel: the shipped DA_FeelConfig is checked by Hollow.Game.Feel.Data.
	constexpr float Inner = 100.f;
	constexpr float Outer = 1000.f;
	constexpr float Frame = 1.f / 60.f;

	FHollowShakeParams Shake()
	{
		FHollowShakeParams Result;
		Result.Duration = 0.4f;
		Result.BlendIn = 0.05f;
		Result.Rotation = FRotator(1.5f, 1.f, 0.5f);
		Result.Location = FVector(0.0, 0.0, 0.5);
		Result.Frequency = 12.f;
		return Result;
	}

	FHollowViewKick Kick(const float Pitch, const float Recover)
	{
		FHollowViewKick Result;
		Result.Pitch = Pitch;
		Result.Yaw = 0.5f;
		Result.PushBack = 3.f;
		Result.Recover = Recover;
		return Result;
	}

	FHollowFeelEntry Personal()
	{
		FHollowFeelEntry Entry;
		Entry.Trigger = HollowTags::Feel_Shot_Shotgun;
		Entry.Shake = Shake();
		Entry.Rumble.Intensity = 0.8f;
		Entry.Rumble.Duration = 0.15f;
		Entry.Kick = Kick(3.f, 0.25f);
		return Entry;
	}

	FHollowFeelEntry Blast()
	{
		FHollowFeelEntry Entry;
		Entry.Trigger = HollowTags::Sound_Mine_Blast;
		Entry.Shake = Shake();
		Entry.InnerRadius = Inner;
		Entry.OuterRadius = Outer;
		return Entry;
	}

	UHollowFeelConfig* Config()
	{
		UHollowFeelConfig* Result = NewObject<UHollowFeelConfig>(GetTransientPackage());
		Result->Entries = {Personal(), Blast()};
		Result->ReducedFlashesScale = 0.4f;
		Result->SwayPerTurnRate = 0.02f;
		Result->MaxSway = 3.f;
		Result->SwayReturnRate = 12.f;
		return Result;
	}

	int32 ActiveShakes(const APlayerController& Player)
	{
		const UCameraModifier_CameraShake* Shakes = Player.PlayerCameraManager
			? Cast<UCameraModifier_CameraShake>(Player.PlayerCameraManager->FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass())) : nullptr;
		TArray<FActiveCameraShakeInfo> Active;
		if (Shakes)
		{
			Shakes->GetActiveCameraShakes(Active);
		}
		return Active.FilterByPredicate([](const FActiveCameraShakeInfo& Info) { return Cast<UHollowFeelCameraShake>(Info.ShakeInstance) != nullptr; }).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFeelRulesTest, "Hollow.Core.Feel.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFeelRulesTest::RunTest(const FString& Parameters)
{
	// GP-01c (ADR-0046): a sound's feel fades with distance, a shake rises, settles and ends, the same seed shakes the
	// same, a kick springs back; a config with holes is refused.
	using namespace HollowFeelTests;
	const FHollowFeelEntry Near = Blast();
	TestEqual(TEXT("Within the inner radius: full"), HollowFeelRules::ScaleAt(Near, Inner * 0.5f), 1.f);
	TestEqual(TEXT("Half way out: half"), HollowFeelRules::ScaleAt(Near, (Inner + Outer) * 0.5f), 0.5f, 1.e-4f);
	TestEqual(TEXT("At the outer radius and past it: nothing"), HollowFeelRules::ScaleAt(Near, Outer) + HollowFeelRules::ScaleAt(Near, Outer * 2.f), 0.f);
	TestEqual(TEXT("One player's own: full anywhere"), HollowFeelRules::ScaleAt(Personal(), Outer * 10.f), 1.f);

	const FHollowShakeParams Params = Shake();
	TestEqual(TEXT("Starts from nothing"), HollowFeelRules::Envelope(Params, 0.f), 0.f);
	TestTrue(TEXT("Strongest once blended in"), HollowFeelRules::Envelope(Params, Params.BlendIn) > HollowFeelRules::Envelope(Params, Params.Duration * 0.5f));
	TestEqual(TEXT("Nothing at its end and after"), HollowFeelRules::Envelope(Params, Params.Duration) + HollowFeelRules::Envelope(Params, Params.Duration + 1.f), 0.f);
	float Previous = 1.f;
	bool bSettles = true;
	for (float Time = Params.BlendIn; Time < Params.Duration; Time += Frame)
	{
		const float Value = HollowFeelRules::Envelope(Params, Time);
		bSettles &= Value <= Previous + UE_KINDA_SMALL_NUMBER;
		Previous = Value;
	}
	TestTrue(TEXT("...and only settles after it"), bSettles);

	FRotator RotationA;
	FRotator RotationB;
	FRotator RotationC;
	FVector Location;
	HollowFeelRules::Sample(Params, 7, 0.1f, RotationA, Location);
	HollowFeelRules::Sample(Params, 7, 0.1f, RotationB, Location);
	HollowFeelRules::Sample(Params, 8, 0.1f, RotationC, Location);
	TestTrue(TEXT("The same seed: the same shake"), RotationA.Equals(RotationB));
	TestFalse(TEXT("Another seed: another shake"), RotationA.Equals(RotationC, 1.e-3f));
	TestTrue(TEXT("Within its swing"), FMath::Abs(RotationA.Pitch) <= Params.Rotation.Pitch && FMath::Abs(RotationA.Roll) <= Params.Rotation.Roll);
	HollowFeelRules::Sample(Params, 7, Params.Duration, RotationA, Location);
	TestTrue(TEXT("Still at its end"), RotationA.IsNearlyZero() && Location.IsNearlyZero());

	const FHollowViewKick Recoil = Kick(2.f, 0.2f);
	TestEqual(TEXT("A kick: all of it at once"), HollowFeelRules::KickLeft(Recoil, 0.f), 1.f);
	TestTrue(TEXT("...springing back"), HollowFeelRules::KickLeft(Recoil, 0.05f) > HollowFeelRules::KickLeft(Recoil, 0.15f));
	TestEqual(TEXT("...at rest by its recovery"), HollowFeelRules::KickLeft(Recoil, 0.2f), 0.f);

	TestTrue(TEXT("A sound config: fine"), HollowFeelRules::Check({Personal(), Blast()}).IsEmpty());
	FHollowFeelEntry Rangeless = Blast();
	Rangeless.OuterRadius = 0.f;
	FHollowFeelEntry Kicking = Blast();
	Kicking.Trigger = HollowTags::Noise_Impact_Heavy;
	Kicking.Kick = Kick(1.f, 0.1f);
	FHollowFeelEntry Idle;
	Idle.Trigger = HollowTags::Feel_Swap;
	TestEqual(TEXT("A sound without range, a sound that kicks, an empty entry, a duplicate"),
		HollowFeelRules::Check({Rangeless, Kicking, Idle, Personal(), Personal()}).Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFeelViewTest, "Hollow.Core.Feel.View",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFeelViewTest::RunTest(const FString& Parameters)
{
	// GP-01c: a kick turns the view at once and springs back, kicks add up and alternate sideways, the hands come back
	// with it and trail the aim while it turns, then settle.
	using namespace HollowFeelTests;
	UHollowViewFeelModifier* View = NewObject<UHollowViewFeelModifier>(GetTransientPackage());
	View->SetConfig(Config());
	int32 Poses = 0;
	View->OnHandsPose.AddLambda([&Poses](const FTransform&) { ++Poses; });
	TestTrue(TEXT("At rest: nothing"), View->Step(Frame, FRotator::ZeroRotator).IsNearlyZero() && Poses == 0);

	View->AddKick(Kick(2.f, 0.2f), 1.f);
	const FRotator First = View->Step(Frame, FRotator::ZeroRotator);
	TestEqual(TEXT("A kick: the view up at once"), First.Pitch, 2.0, 1.e-3);
	TestTrue(TEXT("...the hands back with it"), View->GetHandsPose().GetLocation().X < -2.9 && Poses == 1);
	View->AddKick(Kick(2.f, 0.2f), 0.5f);
	const FRotator Second = View->Step(Frame, FRotator::ZeroRotator);
	TestTrue(TEXT("Another on top: more, to the other side"), Second.Pitch > First.Pitch && Second.Yaw < First.Yaw);
	FRotator Last = Second;
	for (int32 Step = 0; Step < 30; ++Step)
	{
		Last = View->Step(Frame, FRotator::ZeroRotator);
	}
	TestTrue(TEXT("...back at rest by its recovery"), Last.IsNearlyZero() && View->GetKickCount() == 0 && View->GetHandsPose().Equals(FTransform::Identity));
	const int32 PosesAtRest = Poses;
	View->Step(Frame, FRotator::ZeroRotator);
	TestEqual(TEXT("At rest the hands are told once"), Poses, PosesAtRest);
	for (int32 Step = 0; Step < UHollowViewFeelModifier::MaxKicks + 3; ++Step)
	{
		View->AddKick(Kick(1.f, 1.f), 1.f);
	}
	TestEqual(TEXT("At most a few kicks at once"), View->GetKickCount(), UHollowViewFeelModifier::MaxKicks);

	UHollowViewFeelModifier* Hands = NewObject<UHollowViewFeelModifier>(GetTransientPackage());
	Hands->SetConfig(Config());
	Hands->Step(Frame, FRotator::ZeroRotator);
	Hands->Step(Frame, FRotator(0.f, 1.f, 0.f));   // turning right at 60 degrees a second
	TestTrue(TEXT("The hands trail a turn"), Hands->GetSway().Yaw < 0.f && Hands->GetSway().Yaw >= -Config()->MaxSway);
	TestTrue(TEXT("...the view itself does not move"), Hands->GetViewOffset().IsNearlyZero());
	for (int32 Step = 0; Step < 60; ++Step)
	{
		Hands->Step(Frame, FRotator(0.f, 1.f, 0.f));
	}
	TestTrue(TEXT("...and settle once it stops"), FMath::Abs(Hands->GetSway().Yaw) < 0.01f);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		Hands->Step(Frame, FRotator(0.f, 1.f + 30.f * (Step + 1), 0.f));   // a whip: 1800 degrees a second
	}
	TestEqual(TEXT("...never further than their limit"), Hands->GetSway().Yaw, static_cast<double>(-Config()->MaxSway), 0.05);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFeelSubsystemTest, "Hollow.Core.Feel.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFeelSubsystemTest::RunTest(const FString& Parameters)
{
	// GP-01c: a player's own feel reaches its local player only (a shake, its view kick); a sound's is felt by distance
	// when it plays, not past its outer radius; a camera shake is the data's.
	using namespace HollowFeelTests;
	FTestWorldWrapper Wrapper;
	if (!Wrapper.CreateTestWorld(EWorldType::Game))
	{
		Wrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Wrapper.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	UHollowFeelSubsystem* Feel = UHollowFeelSubsystem::Get(World);
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(World);
	if (!TestNotNull(TEXT("The feel subsystem"), Feel) || !TestNotNull(TEXT("The audio subsystem"), Audio))
	{
		return false;
	}
	Feel->SetConfig(Config());
	APawn* Body = World->SpawnActor<APawn>(FVector::ZeroVector, FRotator::ZeroRotator);
	APlayerController* Player = World->SpawnActor<APlayerController>();
	APawn* Stranger = World->SpawnActor<APawn>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestTrue(TEXT("A local player and its body"), Body && Player && Stranger && Player->PlayerCameraManager))
	{
		return false;
	}
	Player->SetAsLocalPlayerController();
	Player->Possess(Body);

	TestTrue(TEXT("Its own shot: felt"), Feel->EmitFor(HollowTags::Feel_Shot_Shotgun, Body));
	TestEqual(TEXT("...as a shake"), ActiveShakes(*Player), 1);
	const UHollowViewFeelModifier* View = Feel->GetViewModifier(*Player);
	TestTrue(TEXT("...and a kick of its view"), View && View->GetKickCount() == 1);
	TestFalse(TEXT("Nobody's body: nothing"), Feel->EmitFor(HollowTags::Feel_Shot_Shotgun, Stranger));
	TestFalse(TEXT("An unknown feel: nothing"), Feel->EmitFor(HollowTags::Feel_Swap, Body));
	TestEqual(TEXT("A sound's feel is not one player's"), Feel->EmitAt(HollowTags::Feel_Shot_Shotgun, FVector::ZeroVector), 0);

	const float Away = (Inner + Outer) * 0.5f;
	Audio->Play(HollowTags::Sound_Mine_Blast, FVector(Away, 0.0, 0.0));
	TestEqual(TEXT("A blast half way out: felt"), Feel->GetFeltCount(HollowTags::Sound_Mine_Blast), 1);
	TestEqual(TEXT("...at half strength"), Feel->GetLastScale(), 0.5f, 1.e-3f);
	TestEqual(TEXT("...one more shake"), ActiveShakes(*Player), 2);
	Audio->Play(HollowTags::Sound_Mine_Blast, FVector(Outer * 2.0, 0.0, 0.0));
	TestEqual(TEXT("Far away: not felt"), Feel->GetFeltCount(HollowTags::Sound_Mine_Blast), 1);
	Audio->Play(HollowTags::Sound_Mine_Arm, FVector::ZeroVector);
	TestEqual(TEXT("A sound without feel: nothing"), Feel->GetFeltCount(HollowTags::Sound_Mine_Arm), 0);

	TArray<FActiveCameraShakeInfo> Active;
	Cast<UCameraModifier_CameraShake>(Player->PlayerCameraManager->FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()))->GetActiveCameraShakes(Active);
	const UHollowFeelCameraShake* Started = Active.IsEmpty() ? nullptr : Cast<UHollowFeelCameraShake>(Active.Last().ShakeInstance);
	TestTrue(TEXT("The shake is the data's"), Started && Started->GetPattern() && Started->GetPattern()->GetShake().Duration == Shake().Duration
		&& Started->GetPattern()->GetShake().Rotation.Equals(Shake().Rotation));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
