#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/DataValidation.h"
#include "Rules/HollowLightBudget.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"

namespace HollowLightBudgetTests
{
	// Test data (the shipped limits live in DA_LightBudget).
	constexpr float Reach = 1000.f;

	FHollowRoomLightLimits Limits(const FGameplayTag& Type, const int32 Lights, const int32 Shadowed, const int32 Flickering, const int32 Functions,
		const int32 Volumetric, const bool bPassThrough = false)
	{
		FHollowRoomLightLimits Result;
		Result.RoomType = Type;
		Result.MaxLights = Lights;
		Result.MaxShadowed = Shadowed;
		Result.MaxShadowedFlickering = Flickering;
		Result.MaxLightFunctions = Functions;
		Result.MaxVolumetricShadows = Volumetric;
		Result.bPassThrough = bPassThrough;
		return Result;
	}

	FHollowLightInfo Light(const bool bShadowed, const bool bFunction = false, const bool bVolumetric = false, const float Radius = Reach * 0.5f)
	{
		FHollowLightInfo Result;
		Result.Name = TEXT("Light");
		Result.bShadowed = bShadowed;
		Result.bLightFunction = bFunction;
		Result.bVolumetricShadow = bVolumetric;
		Result.AttenuationRadius = Radius;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLightBudgetRulesTest, "Hollow.PCG.LightBudget.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLightBudgetRulesTest::RunTest(const FString& Parameters)
{
	// The light budget of a room (ADR-0021): every category is counted against its limit, no light reaches past the
	// room, and the per-type limits keep any view within the frame's shadowed lights (Block 3 sec. 3.9).
	using namespace HollowLightBudgetTests;
	const FHollowRoomLightLimits Big = Limits(HollowTags::Room_Type_CoreVault, 6, 2, 1, 3, 1);
	const FHollowRoomLightLimits Corridor = Limits(HollowTags::Room_Type_Corridor, 3, 0, 0, 2, 0, /*bPassThrough=*/true);

	TestTrue(TEXT("A rig within every limit passes"), HollowLightBudget::CheckRoom(
		{Light(true, false, true), Light(true, true), Light(false, true), Light(false, true), Light(false)}, Big, Reach).IsEmpty());
	TestEqual(TEXT("Too many lights"), HollowLightBudget::CheckRoom({Light(false), Light(false), Light(false), Light(false)}, Corridor, Reach).Num(), 1);
	TestEqual(TEXT("A third shadowed light"), HollowLightBudget::CheckRoom({Light(true), Light(true), Light(true)}, Big, Reach).Num(), 1);
	TestEqual(TEXT("Two shadowed flickering lights"), HollowLightBudget::CheckRoom({Light(true, true), Light(true, true)}, Big, Reach).Num(), 1);
	TestEqual(TEXT("Any shadow in a pass-through room"), HollowLightBudget::CheckRoom({Light(true)}, Corridor, Reach).Num(), 1);
	TestEqual(TEXT("Too many light functions"), HollowLightBudget::CheckRoom({Light(false, true), Light(false, true), Light(false, true)}, Corridor, Reach).Num(), 1);
	TestEqual(TEXT("Two volumetric shadows"), HollowLightBudget::CheckRoom({Light(true, false, true), Light(true, false, true)}, Big, Reach).Num(), 1);
	TestEqual(TEXT("A light reaching past the room"), HollowLightBudget::CheckRoom({Light(false, false, false, Reach * 2.f)}, Big, Reach).Num(), 1);
	TestTrue(TEXT("Without a reach rule, any radius"), HollowLightBudget::CheckRoom({Light(false, false, false, Reach * 2.f)}, Big, 0.f).IsEmpty());

	TestEqual(TEXT("Worst view: two big rooms and a corridor between"), HollowLightBudget::WorstShadowedInView({Big, Corridor, Big}), 4);
	TestEqual(TEXT("A shadowed corridor would add to it"), HollowLightBudget::WorstShadowedInView(
		{Big, Limits(HollowTags::Room_Type_Corridor, 3, 1, 0, 2, 0, true)}), 4);
	TestEqual(TEXT("...two of them more still"), HollowLightBudget::WorstShadowedInView(
		{Big, Limits(HollowTags::Room_Type_Junction, 3, 1, 0, 2, 0, true), Limits(HollowTags::Room_Type_Corridor, 3, 1, 0, 2, 0, true)}), 4);

	UHollowLightBudgetConfig* Config = NewObject<UHollowLightBudgetConfig>(GetTransientPackage());
	Config->Rooms = {Big, Corridor};
	Config->MaxShadowedInView = 4;
	FGameplayTagContainer Tags(HollowTags::Room_Type_Corridor);
	TestTrue(TEXT("Limits are found by the room's type"), Config->FindLimits(Tags) == &Config->Rooms[1]);
	TestNull(TEXT("...or not at all"), Config->FindLimits(FGameplayTagContainer(HollowTags::Room_Type_Hangar)));
	FDataValidationContext Valid;
	TestTrue(TEXT("Shipped-like limits validate"), Config->IsDataValid(Valid) != EDataValidationResult::Invalid);
	Config->Rooms.Add(Limits(HollowTags::Room_Type_Hangar, 6, 3, 1, 3, 1));
	FDataValidationContext Invalid;
	TestTrue(TEXT("A room type that could crowd a view over the frame's limit does not"), Config->IsDataValid(Invalid) == EDataValidationResult::Invalid);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
