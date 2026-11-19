#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/HollowRoomDefinition.h"
#include "Engine/AssetManager.h"
#include "HAL/PlatformTime.h"
#include "Sector/HollowSectorFeatureSubsystem.h"
#include "Tests/AutomationCommon.h"

namespace HollowSectorTests
{
	const TCHAR* const DocksPlugin = TEXT("HollowSector_Docks");
	constexpr int32 DocksMinRooms = 8;
	/** Test data: a content-only plugin transitions within a few frames; this only catches a hang. */
	constexpr double TransitionTimeoutSeconds = 30.0;

	/** Completion of one asynchronous plugin transition. */
	struct FTransition
	{
		bool bDone = false;
		bool bSucceeded = false;
		double Deadline = 0.0;

		UHollowSectorFeatureSubsystem::FOnHollowSectorChanged Start(const TSharedRef<FTransition>& Self)
		{
			bDone = false;
			bSucceeded = false;
			Deadline = FPlatformTime::Seconds() + TransitionTimeoutSeconds;
			return UHollowSectorFeatureSubsystem::FOnHollowSectorChanged::CreateLambda([Self](const bool bOk)
			{
				Self->bSucceeded = bOk;
				Self->bDone = true;
			});
		}

		bool TimedOut() const { return FPlatformTime::Seconds() > Deadline; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSectorActivationTest, "Hollow.Game.Sector.Activation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSectorActivationTest::RunTest(const FString& Parameters)
{
	using namespace HollowSectorTests;

	UHollowSectorFeatureSubsystem* Sectors = UHollowSectorFeatureSubsystem::Get();
	if (!TestNotNull(TEXT("Sector subsystem exists"), Sectors))
	{
		return false;
	}
	if (!TestFalse(TEXT("Docks starts Registered, not Active (BuiltInInitialFeatureState)"), Sectors->IsSectorActive(DocksPlugin)))
	{
		return false;
	}
	TestTrue(TEXT("Registered: the asset manager already knows the Docks rooms"),
		UHollowSectorFeatureSubsystem::GetSectorRooms(DocksPlugin).Num() >= DocksMinRooms);

	const TSharedRef<FTransition> Transition = MakeShared<FTransition>();
	Sectors->ActivateSector(DocksPlugin, Transition->Start(Transition));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Sectors, Transition]()
	{
		if (!Transition->bDone)
		{
			if (Transition->TimedOut())
			{
				AddError(TEXT("Activation did not complete"));
				return true;
			}
			return false;
		}

		TestTrue(TEXT("Activation succeeded"), Transition->bSucceeded);
		TestTrue(TEXT("Docks is Active"), Sectors->IsSectorActive(DocksPlugin));

		const TArray<FPrimaryAssetId> Rooms = UHollowSectorFeatureSubsystem::GetSectorRooms(DocksPlugin);
		TestTrue(FString::Printf(TEXT("At least %d rooms (found %d)"), DocksMinRooms, Rooms.Num()), Rooms.Num() >= DocksMinRooms);
		for (const FPrimaryAssetId& Id : Rooms)
		{
			const UObject* Room = UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad();
			TestNotNull(*FString::Printf(TEXT("%s loads from the plugin"), *Id.ToString()), Cast<UHollowRoomDefinition>(Room));
		}

		Sectors->DeactivateSector(DocksPlugin, Transition->Start(Transition));
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Sectors, Transition]()
	{
		if (!Transition->bDone)
		{
			if (Transition->TimedOut())
			{
				AddError(TEXT("Deactivation did not complete"));
				return true;
			}
			return false;
		}

		TestTrue(TEXT("Deactivation succeeded"), Transition->bSucceeded);
		TestFalse(TEXT("Docks is no longer Active"), Sectors->IsSectorActive(DocksPlugin));
		TestTrue(TEXT("Back to Registered: rooms stay known"), UHollowSectorFeatureSubsystem::GetSectorRooms(DocksPlugin).Num() >= DocksMinRooms);
		return true;
	}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
