#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowSurvivalSet.h"
#include "Audio/HollowSoundBank.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Drone/HollowDroneDefinition.h"
#include "Drone/HollowSoloDrone.h"
#include "Drone/HollowSoloDroneSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
#include "Interaction/HollowInteractionComponent.h"
#include "Misc/ScopeExit.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Movement/HollowMovementTuning.h"
#include "Raid/HollowExtractionZone.h"
#include "Tags/HollowGameplayTags.h"
#include "World/HollowRitualAltar.h"

namespace HollowDroneTests
{
	// Content under test (create_drone_assets.py, create_carry_assets.py, create_raid_assets.py, the Cathedral's mechanics).
	const TCHAR* const DroneClass = TEXT("/Game/Hollow/Drone/BP_SoloDrone.BP_SoloDrone_C");
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const MovementTuningPath = TEXT("/Game/Hollow/Characters/ExoFrame/DA_MovementTuning_ExoFrame.DA_MovementTuning_ExoFrame");
	const TCHAR* const ZoneClass = TEXT("/Game/Hollow/World/Raid/BP_ExtractionZone.BP_ExtractionZone_C");
	const TCHAR* const AltarClass = TEXT("/HollowSector_Cathedral/Mechanics/BP_RitualAltar.BP_RitualAltar_C");
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const DroneMeshName = TEXT("SM_LegacyDrone");
	const TCHAR* const NoiseRoot = TEXT("Hollow.Noise");

	/** Test data (the drone's own numbers are DA_SoloDrone's). */
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float SettleSeconds = 2.f;
	constexpr float CoreDistance = 120.f;
	constexpr float CarrySeconds = 1.5f;
	constexpr double FollowTolerance = 2.0;     // cm from where it hovers, once settled
	constexpr double AngleTolerance = 1.0;      // degrees
	constexpr double Teleport = 300.0;          // cm in one frame
	constexpr double TurnYaw = 90.0;
	constexpr double GlanceYaw = 25.0;          // degrees: well inside the dead zone
	constexpr float FocusSeconds = 0.5f;        // the focus timer (15 Hz) and the heading timer (10 Hz) both run
	constexpr double LagAtLeast = 20.0;         // cm still to go one frame after a jump or a turn
	constexpr double WallGap = 55.0;            // cm right of the capsule centre: between the player and the drone
	const FVector WallScale(4.0, 0.1, 3.0);     // a 10 cm wall, 4 m long and 3 m high (the cube mesh is 100 cm)
	constexpr double WallHalfThickness = 5.0;
	constexpr double DropTolerance = 5.0;       // cm
	constexpr float ZoneExtent = 60.f;          // cm: a zone round the player only, not round the drone beside it
	constexpr double AltarDistance = 250.0;
	constexpr float PaceTolerance = 1e-4f;
	/** DRONE-01 (ADR-0054): the drone carry pace, create_carry_assets.py CARRY_MODES["drone"]. */
	constexpr float DronePace = 0.75f;
	constexpr float DroneAcceleration = 0.8f;

	struct FRig
	{
		FTestWorldWrapper World;
		AHollowExoFrameCharacter* Character = nullptr;
		UHollowAbilitySystemComponent* AbilitySystem = nullptr;
		AHollowAetherCore* Core = nullptr;
		UHollowCarryableComponent* Carryable = nullptr;
		AHollowSoloDrone* Drone = nullptr;
		TSubclassOf<AHollowSoloDrone> DroneType;
	};

	/** One engine frame per step: the timer manager ticks once per GFrameCounter; the drone's arm ticks every frame. */
	void Advance(FRig& Rig, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			Rig.World.TickTestWorld(TickSeconds);
		}
	}

	/** Where the drone should hover: its definition's offset in the player's capsule space. */
	FVector Wanted(const FRig& Rig)
	{
		return Rig.Character->GetActorTransform().TransformPosition(Rig.Drone->GetDefinition()->FollowOffset);
	}

	FVector HoverAt(const FRig& Rig)
	{
		return Rig.Drone->GetHover()->GetComponentLocation();
	}

	/** The player turns to look straight at the drone, as Hollow.Dev.FaceDrone does. */
	void FaceDrone(FRig& Rig)
	{
		FVector Eyes;
		FRotator Unused;
		Rig.Character->GetActorEyesViewPoint(Eyes, Unused);
		const FRotator Look = (HoverAt(Rig) - Eyes).Rotation();
		Rig.Character->GetController()->SetControlRotation(Look);
		Rig.Character->SetActorRotation(FRotator(0.0, Look.Yaw, 0.0));
	}

	/** Whether the player's eyes see the drone on the Interaction channel (what its focus sweeps). */
	bool IsSeenForInteraction(const FRig& Rig)
	{
		FVector Eyes;
		FRotator Unused;
		Rig.Character->GetActorEyesViewPoint(Eyes, Unused);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowDroneTest), /*bTraceComplex=*/false, Rig.Character);
		if (Rig.Core)
		{
			Params.AddIgnoredActor(Rig.Core);
		}
		FHitResult Hit;
		return Rig.World.GetTestWorld()->LineTraceSingleByChannel(Hit, Eyes, HoverAt(Rig), HollowCollision::Interaction, Params) && Hit.GetActor() == Rig.Drone;
	}

	/**
	 * A local player (solo, listen host) standing still (an empty test world has no floor) with real init attributes,
	 * optionally the real BP_AetherCore in front of it, and the real BP_SoloDrone deployed for it and settled.
	 */
	bool Build(FAutomationTestBase& Test, FRig& Rig, const bool bCore)
	{
		UWorld* World = HollowGameTests::CreateWorld(Test, Rig.World);
		Rig.Character = HollowGameTests::SpawnExoFrame(Test, World);
		Rig.DroneType = LoadClass<AHollowSoloDrone>(nullptr, DroneClass);
		const TSubclassOf<UGameplayEffect> InitClass = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
		if (!Test.TestNotNull(TEXT("ExoFrame"), Rig.Character) || !Test.TestNotNull(TEXT("BP_SoloDrone"), Rig.DroneType.Get())
			|| !Test.TestNotNull(TEXT("GE_ExoFrame_InitAttributes"), InitClass.Get()))
		{
			return false;
		}
		// A local player sets up input; the bare C++ class has no InputConfig (BP_ExoFrame data), which it reports.
		Test.AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
		HollowGameTests::PossessAsPlayer(World, Rig.Character, /*bLocalPlayer=*/true);
		Rig.Character->DispatchBeginPlay();
		Rig.Character->GetCharacterMovement()->DisableMovement();
		Rig.AbilitySystem = Cast<UHollowAbilitySystemComponent>(Rig.Character->GetAbilitySystemComponent());
		if (!Test.TestNotNull(TEXT("Ability system on the player state"), Rig.AbilitySystem))
		{
			return false;
		}
		Rig.AbilitySystem->ApplyGameplayEffectToSelf(InitClass.GetDefaultObject(), 1.f, Rig.AbilitySystem->MakeEffectContext());
		if (bCore)
		{
			const TSubclassOf<AHollowAetherCore> Core = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
			if (!Test.TestNotNull(TEXT("BP_AetherCore"), Core.Get()))
			{
				return false;
			}
			Rig.Core = World->SpawnActor<AHollowAetherCore>(Core, FTransform(FVector(CoreDistance, 0.0, 0.0)));
			Rig.Core->DispatchBeginPlay();
			Rig.Carryable = Rig.Core->GetCarryable();
		}
		UHollowSoloDroneSubsystem* Drones = UHollowSoloDroneSubsystem::Get(World);
		Rig.Drone = Drones ? Drones->Deploy({Rig.Character}, Rig.DroneType) : nullptr;
		if (!Test.TestNotNull(TEXT("A solo raid deploys the drone"), Rig.Drone) || !Test.TestNotNull(TEXT("DA_SoloDrone"), Rig.Drone->GetDefinition()))
		{
			return false;
		}
		if (!Rig.Drone->HasActorBegunPlay())
		{
			Rig.Drone->DispatchBeginPlay();   // test worlds do not begin play by themselves
		}
		Advance(Rig, SettleSeconds);
		return true;
	}

	/** Lifts the core alone, then hands it to the drone (the server side of both Interacts). */
	void HandOver(FRig& Rig)
	{
		Rig.Core->Interact(Rig.Character);
		Rig.Drone->Interact(Rig.Character);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDroneSpawnTest, "Hollow.Game.Drone.Spawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDroneSpawnTest::RunTest(const FString& Parameters)
{
	// DRONE-01 (ADR-0054): only a raid of exactly one player gets the drone, never a co-op squad. It serves that player
	// and rides its capsule; no Tick, no collision, no shadow; a soft light and a players-only hum; a second deploy does
	// nothing; it goes when its player goes.
	using namespace HollowDroneTests;
	TestFalse(TEXT("Nobody: no drone"), UHollowSoloDroneSubsystem::IsSoloRaid(0));
	TestTrue(TEXT("One player: the drone"), UHollowSoloDroneSubsystem::IsSoloRaid(1));
	TestFalse(TEXT("Two players: none"), UHollowSoloDroneSubsystem::IsSoloRaid(2));
	TestFalse(TEXT("Four players: none"), UHollowSoloDroneSubsystem::IsSoloRaid(4));
	{
		FTestWorldWrapper CoopWorld;
		UWorld* World = HollowGameTests::CreateWorld(*this, CoopWorld);
		const TSubclassOf<AHollowSoloDrone> Type = LoadClass<AHollowSoloDrone>(nullptr, DroneClass);
		AHollowExoFrameCharacter* First = HollowGameTests::SpawnExoFrame(*this, World);
		AHollowExoFrameCharacter* Second = HollowGameTests::SpawnExoFrame(*this, World, FTransform(FVector(0.0, CoreDistance, 0.0)));
		UHollowSoloDroneSubsystem* Drones = UHollowSoloDroneSubsystem::Get(World);
		if (!TestNotNull(TEXT("Drone subsystem"), Drones) || !TestNotNull(TEXT("BP_SoloDrone"), Type.Get()) || !First || !Second)
		{
			return false;
		}
		HollowGameTests::PossessAsPlayer(World, First);
		HollowGameTests::PossessAsPlayer(World, Second);
		TestNull(TEXT("A co-op raid deploys no drone"), Drones->Deploy({First, Second}, Type));
		TestFalse(TEXT("...none in the world"), static_cast<bool>(TActorIterator<AHollowSoloDrone>(World)));
	}

	FRig Rig;
	if (!Build(*this, Rig, /*bCore=*/false))
	{
		return false;
	}
	AHollowSoloDrone* Drone = Rig.Drone;
	UHollowSoloDroneSubsystem* Drones = UHollowSoloDroneSubsystem::Get(Rig.World.GetTestWorld());
	TestTrue(TEXT("It serves the only player and rides its capsule"), Drone->GetPlayer() == Rig.Character && Drone->GetCarryOwner() == Rig.Character
		&& Drone->GetAttachParentActor() == Rig.Character && Drone->GetOwner() == Rig.Character && Drones->GetDrone() == Drone);
	TestNull(TEXT("One drone a raid"), Drones->Deploy({Rig.Character}, Rig.DroneType));
	TestFalse(TEXT("No Tick of its own (its arm, an engine component, moves it)"), Drone->PrimaryActorTick.bCanEverTick);
	const UStaticMeshComponent* Mesh = Drone->FindComponentByClass<UStaticMeshComponent>();
	TestTrue(TEXT("The Fab drone, enlarged"), Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetName() == DroneMeshName
		&& Mesh->GetRelativeScale3D().X > 1.0);
	TestTrue(TEXT("It meets nothing: no collision, no shadow"), Mesh && Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Mesh->CastShadow);
	const USpotLightComponent* Light = Drone->GetLight();
	TestTrue(TEXT("A soft light that casts no shadow"), !Light->CastShadows && Light->OuterConeAngle > Light->InnerConeAngle && Drone->IsLit());
	const UHollowSoundBank* Bank = UHollowSoundBank::Get();
	TestTrue(TEXT("Its hum is in the sound bank"), Bank && Bank->Find(HollowTags::Sound_Drone_Hum));
	TestFalse(TEXT("...and is no noise the AI hears"), HollowTags::Sound_Drone_Hum.GetTag().MatchesTag(UGameplayTagsManager::Get().RequestGameplayTag(NoiseRoot)));
	TestFalse(TEXT("Empty-handed there is nothing to do with it"), Drone->CanInteract(Rig.Character) || Drone->IsFocusable());
	TestFalse(TEXT("...so it hides nothing behind it from the player's focus"), IsSeenForInteraction(Rig));

	Rig.Character->Destroy();
	TestTrue(TEXT("It goes when its player goes"), !IsValid(Drone) || Drone->IsActorBeingDestroyed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDroneFollowTest, "Hollow.Game.Drone.Follow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDroneFollowTest::RunTest(const FString& Parameters)
{
	// DRONE-01 (ADR-0054): it hovers right-front of the view at about eye level, facing where the player faces, out of
	// the line of fire; it trails a jump or a turn for a moment (no further than its lag allows) and catches up; a wall in
	// the way keeps it on the player's side.
	using namespace HollowDroneTests;
	FRig Rig;
	if (!Build(*this, Rig, /*bCore=*/false))
	{
		return false;
	}
	AHollowExoFrameCharacter* Character = Rig.Character;
	const UHollowDroneDefinition* Definition = Rig.Drone->GetDefinition();
	TestEqual(TEXT("Settled where it hovers"), HoverAt(Rig), Wanted(Rig), FollowTolerance);
	const FVector Local = Character->GetActorTransform().InverseTransformPosition(HoverAt(Rig));
	TestTrue(TEXT("Right-front of the player, above the capsule centre"), Local.X > 0.0 && Local.Y > 0.0 && Local.Z > 0.0);
	TestEqual(TEXT("Facing where the player faces"), FMath::FindDeltaAngleDegrees(Rig.Drone->GetHover()->GetComponentRotation().Yaw, Character->GetActorRotation().Yaw),
		0.0, AngleTolerance);
	TestTrue(TEXT("Level: no pitch, no roll"), FMath::Abs(Rig.Drone->GetHover()->GetComponentRotation().Pitch) < AngleTolerance
		&& FMath::Abs(Rig.Drone->GetHover()->GetComponentRotation().Roll) < AngleTolerance);
	FVector Eyes;
	FRotator View;
	Character->GetActorEyesViewPoint(Eyes, View);
	const double OffAim = FMath::PointDistToLine(HoverAt(Rig), View.Vector(), Eyes);
	TestTrue(FString::Printf(TEXT("Out of the line of fire (%.0f cm off the aim)"), OffAim), OffAim > 2.0 * Definition->FocusRadius);

	Character->SetActorLocation(Character->GetActorLocation() + FVector(Teleport, 0.0, 0.0));
	Advance(Rig, TickSeconds);
	const double Behind = FVector::Dist(HoverAt(Rig), Wanted(Rig));
	TestTrue(FString::Printf(TEXT("A jump: it trails a moment (%.0f cm), no further than its lag allows"), Behind),
		Behind > LagAtLeast && Behind <= Definition->MaxLagDistance + FollowTolerance);
	Advance(Rig, SettleSeconds);
	TestEqual(TEXT("...and catches up"), HoverAt(Rig), Wanted(Rig), FollowTolerance);

	// A glance aside, and turning to face it: inside the dead zone it stays where it is, so it can be focused.
	const FVector BeforeGlance = HoverAt(Rig);
	Character->GetController()->SetControlRotation(FRotator(0.0, GlanceYaw, 0.0));
	Character->SetActorRotation(FRotator(0.0, GlanceYaw, 0.0));
	Advance(Rig, SettleSeconds);
	TestEqual(TEXT("A glance aside does not move it"), HoverAt(Rig), BeforeGlance, FollowTolerance);
	FaceDrone(Rig);
	Advance(Rig, SettleSeconds);
	TestEqual(TEXT("Turning to face it does not chase it away"), HoverAt(Rig), BeforeGlance, FollowTolerance);
	Character->GetActorEyesViewPoint(Eyes, View);
	TestTrue(TEXT("...it is in the middle of the view"), FMath::PointDistToLine(HoverAt(Rig), View.Vector(), Eyes) < Definition->FocusRadius);

	const FRotator Turned(0.0, TurnYaw, 0.0);
	Character->GetController()->SetControlRotation(Turned);
	Character->SetActorRotation(Turned);
	Advance(Rig, TickSeconds);
	TestTrue(TEXT("A turn: it swings round after the player"), FVector::Dist(HoverAt(Rig), Wanted(Rig)) > LagAtLeast);
	Advance(Rig, SettleSeconds);
	TestEqual(TEXT("...to its place on the new side"), HoverAt(Rig), Wanted(Rig), FollowTolerance);
	TestEqual(TEXT("...facing the new way"), FMath::FindDeltaAngleDegrees(Rig.Drone->GetHover()->GetComponentRotation().Yaw, TurnYaw), 0.0, AngleTolerance);

	const FTransform Capsule = Character->GetActorTransform();
	AStaticMeshActor* Wall = Rig.World.GetTestWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
		FTransform(Capsule.Rotator(), Capsule.TransformPosition(FVector(Definition->FollowOffset.X, WallGap, 0.0)), WallScale));
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);   // a registered static component refuses meshes in play
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMeshPath));
	Advance(Rig, SettleSeconds);
	const FVector Kept = Capsule.InverseTransformPosition(HoverAt(Rig));
	TestTrue(FString::Printf(TEXT("A wall in the way keeps it on the player's side (%.0f cm right, the wall at %.0f)"), Kept.Y, WallGap),
		Kept.Y + Definition->ProbeRadius <= WallGap - WallHalfThickness + FollowTolerance);
	TestTrue(TEXT("...pushed in towards the player"), Kept.Size() < Definition->FollowOffset.Size());
	Wall->Destroy();
	Advance(Rig, SettleSeconds);
	TestEqual(TEXT("The wall gone, it is back in its place"), HoverAt(Rig), Wanted(Rig), FollowTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDroneCarryTest, "Hollow.Game.Drone.Carry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDroneCarryTest::RunTest(const FString& Parameters)
{
	// DRONE-01 (ADR-0054): a player carrying a core alone hands it to the drone (Interact on it, a carry receiver): the
	// core hangs under the drone, the player keeps pace (Drone claim, DA_MovementTuning_ExoFrame: 0.75, a hop allowed,
	// no sprint) with hands free (no heavy-carry tag: tools, weapons and interactions work, stamina is not drained) and
	// Resonance still rises; Interact elsewhere does not drop it; the extraction counts it with its carrier, the altar
	// takes it; Interact on the drone takes it back; the player dying drops it from under the drone, which goes dark.
	using namespace HollowDroneTests;
	FRig Rig;
	if (!Build(*this, Rig, /*bCore=*/true))
	{
		return false;
	}
	AHollowExoFrameCharacter* Character = Rig.Character;
	AHollowSoloDrone* Drone = Rig.Drone;
	const UHollowDroneDefinition* Definition = Drone->GetDefinition();
	UHollowCarrierComponent* Carrier = Character->GetCarrierComponent();
	UHollowCharacterMovementComponent* Movement = Character->GetHollowMovement();
	UHollowInteractionComponent* Interaction = Character->GetInteractionComponent();
	UBoxComponent* Body = Rig.Core->GetBody();
	const FHollowCarryNetState& State = Rig.Carryable->GetCarryState();

	const UHollowMovementTuning* Tuning = LoadObject<UHollowMovementTuning>(nullptr, MovementTuningPath);
	if (!TestNotNull(TEXT("DA_MovementTuning_ExoFrame"), Tuning))
	{
		return false;
	}
	const FHollowCarryModeTuning& Pace = Tuning->Carry.Drone;
	TestTrue(TEXT("The drone carry pace: 0.75, a hop allowed, no sprint"), FMath::IsNearlyEqual(Pace.SpeedScale, DronePace, PaceTolerance)
		&& FMath::IsNearlyEqual(Pace.AccelerationScale, DroneAcceleration, PaceTolerance) && Pace.bAllowJump && !Pace.bAllowSprint);
	TestTrue(TEXT("...faster than carrying alone"), Pace.SpeedScale > Tuning->Carry.Solo.SpeedScale);

	Rig.Core->Interact(Character);
	TestTrue(TEXT("Carrying alone: the drone takes it"), Drone->CanInteract(Character) && Drone->IsFocusable()
		&& Drone->GetInteractionInfo(Character).Prompt.EqualTo(Definition->HandOverPrompt));
	TestTrue(TEXT("...the player's focus finds it"), IsSeenForInteraction(Rig));
	TestFalse(TEXT("...a carrier may hand it over (a carry receiver)"), Interaction->MeasureRequest(Drone).bInteractorBlocked);
	FText Shown;   // what the visor's prompt shows (UHollowPromptModel listens the same way)
	const FDelegateHandle PromptHandle = Interaction->OnFocusChanged.AddLambda([&Shown](AActor*, const FHollowInteractionInfo& Info) { Shown = Info.Prompt; });
	ON_SCOPE_EXIT
	{
		Interaction->OnFocusChanged.Remove(PromptHandle);
	};
	FaceDrone(Rig);
	Advance(Rig, FocusSeconds);   // the player's own focus timer probes the view
	TestTrue(TEXT("Turning to face it, the player focuses it (it does not swing away)"), Interaction->GetFocusedActor() == Drone
		&& Shown.EqualTo(Definition->HandOverPrompt));

	Drone->Interact(Character);
	TestTrue(TEXT("Handed over: Drone mode, its player still the carrier"), State.Mode == EHollowCarryMode::Drone && State.Primary == Character
		&& State.Vehicle == Drone && Carrier->GetCarried() == Rig.Carryable && Drone->GetCarried() == Rig.Carryable);
	TestTrue(TEXT("The core hangs under the drone"), Rig.Carryable->GetPresentation() == EHollowCarryPresentation::Drone
		&& Body->GetAttachParent() == Drone->GetCarryPoint() && Rig.Core->GetVisual()->GetAttachParent() == Body
		&& Rig.Core->GetVisual()->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
	TestEqual(TEXT("...at its carry point"), Body->GetComponentLocation(), Drone->GetCarryPoint()->GetComponentLocation(), FollowTolerance);
	TestTrue(TEXT("...below it"), Body->GetComponentLocation().Z < HoverAt(Rig).Z);
	TestTrue(TEXT("Hands free: the drone's tag, no heavy-carry tag"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_DroneCarry)
		&& !Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_CarryingHeavy));
	TestFalse(TEXT("...tools and weapons work"), Rig.AbilitySystem->AreAbilityTagsBlocked(FGameplayTagContainer(HollowTags::Ability_Tool_Welder))
		|| Rig.AbilitySystem->AreAbilityTagsBlocked(FGameplayTagContainer(HollowTags::Ability_Weapon)));
	TestFalse(TEXT("...and so do interactions"), Interaction->MeasureRequest(Rig.Core).bInteractorBlocked);
	TestTrue(TEXT("The player claims the drone pace; the server knows it"), Movement->GetClaimedCarry() == EHollowCarryClaim::Drone
		&& Movement->GetServerCarry().Current == EHollowCarryClaim::Drone && Movement->GetEffectiveCarry() == EHollowCarryClaim::Drone);
	TestTrue(TEXT("Its prompt now takes it back"), Drone->GetInteractionInfo(Character).Prompt.EqualTo(Definition->TakeBackPrompt));
	Advance(Rig, FocusSeconds);
	TestTrue(TEXT("...and the visor shows it, the drone still in focus"), Interaction->GetFocusedActor() == Drone && Shown.EqualTo(Definition->TakeBackPrompt));
	TestFalse(TEXT("Interact elsewhere goes to the focus, it does not drop the core"), Carrier->HandleInteractInput(nullptr));

	const float StaminaAtHandOver = Rig.AbilitySystem->GetNumericAttribute(UHollowSurvivalSet::GetStaminaAttribute());
	const float ResonanceAtHandOver = Rig.AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetResonanceAttribute());
	Advance(Rig, CarrySeconds);
	TestTrue(TEXT("Still carried by the drone"), State.Mode == EHollowCarryMode::Drone);
	TestTrue(TEXT("No stamina drain"), Rig.AbilitySystem->GetNumericAttribute(UHollowSurvivalSet::GetStaminaAttribute()) >= StaminaAtHandOver);
	TestTrue(TEXT("Resonance still rises (the core is near)"), Rig.AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetResonanceAttribute()) > ResonanceAtHandOver);
	TestEqual(TEXT("It follows under the drone"), Body->GetComponentLocation(), Drone->GetCarryPoint()->GetComponentLocation(), FollowTolerance);

	// The extraction counts it with its carrier, even where the zone ends between them.
	const TSubclassOf<AHollowExtractionZone> ZoneType = LoadClass<AHollowExtractionZone>(nullptr, ZoneClass);
	AHollowExtractionZone* Zone = ZoneType ? Rig.World.GetTestWorld()->SpawnActor<AHollowExtractionZone>(ZoneType, Character->GetActorTransform()) : nullptr;
	if (!TestNotNull(TEXT("BP_ExtractionZone"), Zone))
	{
		return false;
	}
	Zone->GetVolume()->SetBoxExtent(FVector(ZoneExtent));
	TArray<APawn*> Pawns;
	TArray<UHollowCarryableComponent*> Carryables;
	Zone->Sample(Pawns, Carryables);
	TestTrue(TEXT("A zone round the player only: the core under the drone lies outside it"),
		!Zone->GetVolume()->Bounds.GetBox().IsInside(Body->GetComponentLocation()));
	TestTrue(TEXT("...yet the extraction counts it with its carrier"), Pawns.Contains(Character) && Carryables.Contains(Rig.Carryable));
	Zone->Destroy();

	Drone->Interact(Character);
	TestTrue(TEXT("Taken back: carried alone again"), State.Mode == EHollowCarryMode::Solo && State.Primary == Character && !State.Vehicle
		&& !Drone->GetCarried() && Rig.Carryable->GetPresentation() == EHollowCarryPresentation::FirstPerson);
	TestTrue(TEXT("...both hands full again"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_CarryingHeavy_Solo)
		&& !Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_DroneCarry)
		&& Rig.AbilitySystem->AreAbilityTagsBlocked(FGameplayTagContainer(HollowTags::Ability_Tool_Welder)));
	TestTrue(TEXT("...at the solo pace"), Movement->GetClaimedCarry() == EHollowCarryClaim::Solo && Movement->GetServerCarry().Current == EHollowCarryClaim::Solo);

	Drone->Interact(Character);
	TestTrue(TEXT("Handed over again"), State.Mode == EHollowCarryMode::Drone);
	Advance(Rig, TickSeconds);
	const FVector Under = Drone->GetCarryPoint()->GetComponentLocation();
	Character->Die();
	TestFalse(TEXT("Its player died: the core is dropped"), State.IsCarried() || Carrier->GetCarried());
	TestEqual(TEXT("...from under the drone"), FVector(Body->GetComponentLocation().X, Body->GetComponentLocation().Y, 0.0), FVector(Under.X, Under.Y, 0.0),
		DropTolerance);
	TestTrue(TEXT("...falling free"), Body->IsSimulatingPhysics() && !Body->GetAttachParent());
	TestFalse(TEXT("The drone goes dark and cannot be focused"), Drone->IsLit() || Drone->IsFocusable() || Drone->CanInteract(Character));

	// The Purification Altar takes a core the drone carries (its player is the carrier).
	FRig AltarRig;
	if (!Build(*this, AltarRig, /*bCore=*/true))
	{
		return false;
	}
	const TSubclassOf<AHollowRitualAltar> AltarType = LoadClass<AHollowRitualAltar>(nullptr, AltarClass);
	AHollowRitualAltar* Altar = AltarType ? AltarRig.World.GetTestWorld()->SpawnActor<AHollowRitualAltar>(AltarType,
		FTransform(FRotator(0.0, 180.0, 0.0), FVector(AltarDistance, 0.0, 0.0))) : nullptr;
	if (!TestNotNull(TEXT("BP_RitualAltar"), Altar))
	{
		return false;
	}
	Altar->DispatchBeginPlay();
	HandOver(AltarRig);
	TestTrue(TEXT("The drone carries it"), AltarRig.Carryable->GetCarryState().Mode == EHollowCarryMode::Drone);
	TestTrue(TEXT("The altar takes it from the drone's player"), Altar->CanInteract(AltarRig.Character)
		&& !AltarRig.Character->GetInteractionComponent()->MeasureRequest(Altar).bInteractorBlocked);
	Altar->Interact(AltarRig.Character);
	TestTrue(TEXT("...laid on the bowl, held; the drone and its player hold nothing"), AltarRig.Carryable->IsHeld() && !AltarRig.Carryable->GetCarryState().IsCarried()
		&& !AltarRig.Drone->GetCarried() && !AltarRig.Character->GetCarrierComponent()->GetCarried()
		&& !AltarRig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_DroneCarry));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
