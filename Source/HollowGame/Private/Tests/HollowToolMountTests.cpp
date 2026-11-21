#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HollowToolDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Tools/HollowToolAim.h"
#include "Tools/HollowToolMountComponent.h"
#include "UObject/Package.h"

namespace HollowToolMountTests
{
	const TCHAR* const CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FName Muzzle(TEXT("Muzzle"));
	/** Test data: a muzzle off the view axis (the first-person field of view moves only what is off it). */
	const FVector MuzzleOffset(60.0, 25.0, -12.0);
	const FVector FallbackOffset(30.0, 10.0, -5.0);
	/** Test data: the hands' own field of view, narrower than the view's (BP_ExoFrame sets the real one). */
	constexpr float HandsFieldOfView = 70.f;
	constexpr double Tolerance = 0.01;

	/** A transient held mesh: the engine cube with a muzzle socket. */
	UStaticMesh* MakeHeldMesh(FAutomationTestBase& Test)
	{
		const UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubeMesh);
		if (!Test.TestNotNull(TEXT("Engine cube"), Cube))
		{
			return nullptr;
		}
		UStaticMesh* Mesh = DuplicateObject(Cube, GetTransientPackage());
		UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(Mesh);
		Socket->SocketName = Muzzle;
		Socket->RelativeLocation = MuzzleOffset;
		Mesh->AddSocket(Socket);
		return Mesh;
	}

	UHollowToolDefinition* MakeTool(UStaticMesh* Mesh, const FName Socket)
	{
		UHollowToolDefinition* Tool = NewObject<UHollowToolDefinition>(GetTransientPackage());
		Tool->HeldMesh = Mesh;
		Tool->MuzzleSocket = Socket;
		return Tool;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowToolMountTest, "Hollow.Game.Tool.Mount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowToolMountTest::RunTest(const FString& Parameters)
{
	// ADR-0027: the ExoFrame holds its tool in the first-person hands (first-person rendering, owner only) and on the
	// body (everyone else, the owner's shadow); the beam starts at the held tool's muzzle as this machine sees it, and
	// in front of the eyes while nothing with a muzzle is held.
	using namespace HollowToolMountTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Own = HollowGameTests::SpawnExoFrame(*this, World);
	AHollowExoFrameCharacter* Other = HollowGameTests::SpawnExoFrame(*this, World, FTransform(FVector(500.0, 0.0, 0.0)));
	UStaticMesh* Held = MakeHeldMesh(*this);
	if (!TestNotNull(TEXT("ExoFrames"), Own) || !TestNotNull(TEXT("...two"), Other) || !Held)
	{
		return false;
	}
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	HollowGameTests::PossessAsPlayer(World, Own, /*bLocalPlayer=*/true);
	HollowGameTests::PossessAsPlayer(World, Other, /*bLocalPlayer=*/false);   // a remote player as this machine sees it
	UHollowToolMountComponent* Mount = Own->GetToolMount();
	UHollowToolMountComponent* OtherMount = Other->GetToolMount();
	if (!TestNotNull(TEXT("Tool mount"), Mount) || !TestNotNull(TEXT("...on the other too"), OtherMount))
	{
		return false;
	}
	UStaticMeshComponent* FirstPerson = Mount->GetFirstPersonMesh();
	UStaticMeshComponent* Body = Mount->GetThirdPersonMesh();
	TestTrue(TEXT("First-person tool: owner only, first-person rendering"), FirstPerson && FirstPerson->bOnlyOwnerSee
		&& FirstPerson->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson);
	TestTrue(TEXT("Body tool: the world's (the owner sees its shadow)"), Body && Body->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	// The bare class has no meshes, so no hands to sit in; with meshes the tools sit in their sockets (BP_ExoFrame, ExoFrame.Data).
	TestTrue(TEXT("...without hand meshes they stay on the body's root"), FirstPerson && Body && FirstPerson->GetAttachParent() == Own->GetRootComponent()
		&& Body->GetAttachParent() == Own->GetRootComponent());
	TestTrue(TEXT("...neither collides nor affects navigation"), FirstPerson && Body && FirstPerson->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& Body->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !FirstPerson->CanEverAffectNavigation() && !Body->CanEverAffectNavigation());
	TestTrue(TEXT("Own pawn: first-person view; the other: not"), Mount->IsFirstPersonView() && !OtherMount->IsFirstPersonView());

	// Nothing held: no muzzle, the beam leaves in front of the eyes.
	FVector Location;
	TestFalse(TEXT("Empty hands: no muzzle"), Mount->GetMuzzleLocation(Location));
	FHollowToolAimTrace Trace;
	Trace.Eyes = Own->GetPawnViewLocation();
	Trace.Aim = FRotator(0.0, 30.0, 0.0);
	TestTrue(TEXT("...the beam starts at the fallback"), HollowToolAim::BeamStart(*Own, Trace, FallbackOffset).Equals(Trace.Eyes + Trace.Aim.RotateVector(FallbackOffset), Tolerance));

	// A held tool with a muzzle: shown in both hands; the owner's beam starts where its first-person muzzle appears.
	UHollowToolDefinition* Tool = MakeTool(Held, Muzzle);
	Mount->ShowTool(Tool);
	OtherMount->ShowTool(Tool);
	TestTrue(TEXT("Held in both hands"), Mount->GetShownTool() == Tool && FirstPerson->GetStaticMesh() == Held && Body->GetStaticMesh() == Held
		&& FirstPerson->IsVisible() && Body->IsVisible());
	Own->GetFirstPersonCamera()->SetFirstPersonFieldOfView(HandsFieldOfView);
	FMinimalViewInfo View;
	Own->GetFirstPersonCamera()->GetCameraView(0.f, View);
	const FVector Raw = FirstPerson->GetSocketLocation(Muzzle);
	const FVector Seen = View.TransformWorldToFirstPerson(Raw, /*bIgnoreFirstPersonScale=*/true);
	TestTrue(TEXT("Own muzzle: where first-person rendering shows it"), Mount->GetMuzzleLocation(Location) && Location.Equals(Seen, Tolerance));
	TestTrue(TEXT("...which is not where the socket is (own field of view)"), View.bUseFirstPersonParameters && !Seen.Equals(Raw, Tolerance));
	TestTrue(TEXT("...and the beam starts there"), HollowToolAim::BeamStart(*Own, Trace, FallbackOffset).Equals(Seen, Tolerance));
	TestTrue(TEXT("Another player's muzzle: the body tool's socket as it is"), OtherMount->GetMuzzleLocation(Location)
		&& Location.Equals(OtherMount->GetThirdPersonMesh()->GetSocketLocation(Muzzle), Tolerance));

	// A held mesh without the muzzle socket, and empty hands again: the fallback.
	Mount->ShowTool(MakeTool(Held, TEXT("NoSuchSocket")));
	TestFalse(TEXT("No muzzle socket: no muzzle"), Mount->GetMuzzleLocation(Location));
	Mount->ShowTool(nullptr);
	TestTrue(TEXT("Empty hands: nothing shown"), !FirstPerson->IsVisible() && !Body->IsVisible() && !Mount->GetMuzzleLocation(Location));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
