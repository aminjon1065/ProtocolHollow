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

namespace HollowSectorTests
{
	/** Transitions one step of a switch starts, and what is checked once they are all done. */
	using FStartStep = TFunction<void(TArray<TSharedRef<FTransition>>&)>;

	/** One latent step: starts its transitions, waits for every one (or the timeout), then checks. */
	void QueueStep(FAutomationTestBase& Test, const FString& Label, FStartStep Start, TFunction<void()> Check)
	{
		const TSharedRef<TArray<TSharedRef<FTransition>>> Transitions = MakeShared<TArray<TSharedRef<FTransition>>>();
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Transitions, Start]()
		{
			Start(*Transitions);
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([&Test, Label, Transitions, Check]()
		{
			for (const TSharedRef<FTransition>& Transition : *Transitions)
			{
				if (!Transition->bDone)
				{
					if (Transition->TimedOut())
					{
						Test.AddError(FString::Printf(TEXT("%s: a transition did not complete"), *Label));
						return true;
					}
					return false;
				}
				Test.TestTrue(FString::Printf(TEXT("%s: every transition succeeded"), *Label), Transition->bSucceeded);
			}
			Check();
			return true;
		}));
	}

	/** A transition's completion, kept with the step's others. */
	UHollowSectorFeatureSubsystem::FOnHollowSectorChanged Track(TArray<TSharedRef<FTransition>>& Transitions)
	{
		const TSharedRef<FTransition> Transition = MakeShared<FTransition>();
		Transitions.Add(Transition);
		return Transition->Start(Transition);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSectorSwitchTest, "Hollow.Game.Sector.Switch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSectorSwitchTest::RunTest(const FString& Parameters)
{
	// ADR-0041: one session goes Docks -> hub -> Cathedral -> hub. Leaving a raid releases its sector while the next
	// raid's sector activates (the travel overlaps them); each plugin keeps its own references and state, and every
	// sector ends Registered with no reference left.
	using namespace HollowSectorTests;
	UHollowSectorFeatureSubsystem* Sectors = UHollowSectorFeatureSubsystem::Get();
	if (!TestNotNull(TEXT("Sector subsystem exists"), Sectors) || !TestTrue(TEXT("Both sectors start Registered, unused"),
		!Sectors->IsSectorActive(DocksPlugin) && !Sectors->IsSectorActive(CathedralPlugin) && Sectors->GetReferences(DocksPlugin) == 0
		&& Sectors->GetReferences(CathedralPlugin) == 0))
	{
		return false;
	}
	const auto State = [this, Sectors](const TCHAR* Step, const bool bDocks, const int32 DocksRefs, const bool bCathedral, const int32 CathedralRefs)
	{
		TestTrue(FString::Printf(TEXT("%s: Docks active %d with %d reference(s) (%s)"), Step, bDocks, DocksRefs, *Sectors->DescribeSector(DocksPlugin)),
			Sectors->IsSectorActive(DocksPlugin) == bDocks && Sectors->GetReferences(DocksPlugin) == DocksRefs);
		TestTrue(FString::Printf(TEXT("%s: Cathedral active %d with %d reference(s) (%s)"), Step, bCathedral, CathedralRefs, *Sectors->DescribeSector(CathedralPlugin)),
			Sectors->IsSectorActive(CathedralPlugin) == bCathedral && Sectors->GetReferences(CathedralPlugin) == CathedralRefs);
		TestTrue(FString::Printf(TEXT("%s: both sectors' rooms stay known"), Step), UHollowSectorFeatureSubsystem::GetSectorRooms(DocksPlugin).Num() >= DocksMinRooms
			&& UHollowSectorFeatureSubsystem::GetSectorRooms(CathedralPlugin).Num() == CathedralRooms);
	};

	QueueStep(*this, TEXT("Raid in the Docks"), [Sectors](TArray<TSharedRef<FTransition>>& T) { Sectors->ActivateSector(DocksPlugin, Track(T)); },
		[State]() { State(TEXT("Raid in the Docks"), true, 1, false, 0); });
	QueueStep(*this, TEXT("Docks released while the Cathedral activates"), [Sectors](TArray<TSharedRef<FTransition>>& T)
		{
			Sectors->DeactivateSector(DocksPlugin, Track(T));
			Sectors->ActivateSector(CathedralPlugin, Track(T));
		},
		[State]() { State(TEXT("Raid in the Cathedral"), false, 0, true, 1); });
	QueueStep(*this, TEXT("A second game instance in the Cathedral"), [Sectors](TArray<TSharedRef<FTransition>>& T)
		{
			Sectors->ActivateSector(CathedralPlugin, Track(T));
			Sectors->DeactivateSector(CathedralPlugin, Track(T));
		},
		[State]() { State(TEXT("One instance left the Cathedral"), false, 0, true, 1); });
	QueueStep(*this, TEXT("Back to the hub"), [Sectors](TArray<TSharedRef<FTransition>>& T) { Sectors->DeactivateSector(CathedralPlugin, Track(T)); },
		[State]() { State(TEXT("Hub"), false, 0, false, 0); });
	QueueStep(*this, TEXT("The Docks again"), [Sectors](TArray<TSharedRef<FTransition>>& T) { Sectors->ActivateSector(DocksPlugin, Track(T)); },
		[State]() { State(TEXT("The Docks again"), true, 1, false, 0); });
	QueueStep(*this, TEXT("Home"), [Sectors](TArray<TSharedRef<FTransition>>& T) { Sectors->DeactivateSector(DocksPlugin, Track(T)); },
		[State]() { State(TEXT("Hub again"), false, 0, false, 0); });
	return true;
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
