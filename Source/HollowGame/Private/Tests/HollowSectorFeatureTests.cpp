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
	/** Sector II (S7-01, ADR-0038). */
	const TCHAR* const CathedralPlugin = TEXT("HollowSector_Cathedral");
	constexpr int32 CathedralRooms = 8;
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

	/**
	 * ADR-0012 for one sector plugin, as latent commands: Registered at start (its rooms and biome already known), Active
	 * on request (the rooms load from the plugin), back to Registered on release (still known).
	 */
	void QueueSectorLifecycle(FAutomationTestBase& Test, UHollowSectorFeatureSubsystem* Sectors, const TCHAR* Plugin, const int32 MinRooms)
	{
		const TSharedRef<FTransition> Transition = MakeShared<FTransition>();
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([&Test, Sectors, Plugin, MinRooms, Transition]()
		{
			Test.TestFalse(FString::Printf(TEXT("%s starts Registered, not Active (BuiltInInitialFeatureState)"), Plugin), Sectors->IsSectorActive(Plugin));
			Test.TestTrue(FString::Printf(TEXT("%s registered: the asset manager already knows its rooms"), Plugin),
				UHollowSectorFeatureSubsystem::GetSectorRooms(Plugin).Num() >= MinRooms);
			Test.TestTrue(FString::Printf(TEXT("%s registered: its biome is known"), Plugin), UHollowSectorFeatureSubsystem::GetSectorBiome(Plugin).IsValid());
			Sectors->ActivateSector(Plugin, Transition->Start(Transition));
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([&Test, Sectors, Plugin, MinRooms, Transition]()
		{
			if (!Transition->bDone)
			{
				if (Transition->TimedOut())
				{
					Test.AddError(FString::Printf(TEXT("%s: activation did not complete"), Plugin));
					return true;
				}
				return false;
			}

			Test.TestTrue(FString::Printf(TEXT("%s: activation succeeded"), Plugin), Transition->bSucceeded);
			Test.TestTrue(FString::Printf(TEXT("%s is Active"), Plugin), Sectors->IsSectorActive(Plugin));

			const TArray<FPrimaryAssetId> Rooms = UHollowSectorFeatureSubsystem::GetSectorRooms(Plugin);
			Test.TestTrue(FString::Printf(TEXT("%s: at least %d rooms (found %d)"), Plugin, MinRooms, Rooms.Num()), Rooms.Num() >= MinRooms);
			for (const FPrimaryAssetId& Id : Rooms)
			{
				const UObject* Room = UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad();
				Test.TestNotNull(*FString::Printf(TEXT("%s loads from the plugin"), *Id.ToString()), Cast<UHollowRoomDefinition>(Room));
			}

			Sectors->DeactivateSector(Plugin, Transition->Start(Transition));
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([&Test, Sectors, Plugin, MinRooms, Transition]()
		{
			if (!Transition->bDone)
			{
				if (Transition->TimedOut())
				{
					Test.AddError(FString::Printf(TEXT("%s: deactivation did not complete"), Plugin));
					return true;
				}
				return false;
			}

			Test.TestTrue(FString::Printf(TEXT("%s: deactivation succeeded"), Plugin), Transition->bSucceeded);
			Test.TestFalse(FString::Printf(TEXT("%s is no longer Active"), Plugin), Sectors->IsSectorActive(Plugin));
			Test.TestTrue(FString::Printf(TEXT("%s back to Registered: rooms stay known"), Plugin), UHollowSectorFeatureSubsystem::GetSectorRooms(Plugin).Num() >= MinRooms);
			return true;
		}));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSectorActivationTest, "Hollow.Game.Sector.Activation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSectorActivationTest::RunTest(const FString& Parameters)
{
	// Both sectors follow the same lifecycle; the Cathedral (S7-01) is a second plugin beside the Docks.
	using namespace HollowSectorTests;

	UHollowSectorFeatureSubsystem* Sectors = UHollowSectorFeatureSubsystem::Get();
	if (!TestNotNull(TEXT("Sector subsystem exists"), Sectors))
	{
		return false;
	}
	QueueSectorLifecycle(*this, Sectors, DocksPlugin, DocksMinRooms);
	QueueSectorLifecycle(*this, Sectors, CathedralPlugin, CathedralRooms);
	TestEqual(TEXT("The Cathedral has exactly its 8 rooms"), UHollowSectorFeatureSubsystem::GetSectorRooms(CathedralPlugin).Num(), CathedralRooms);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
