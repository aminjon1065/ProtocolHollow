// Development console commands for the loot crates (GP-01b, ADR-0045): what they hold and what lies on the floor. Never shipped.
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Inventory/HollowLoadoutRules.h"
#include "Items/HollowItemDefinition.h"
#include "World/HollowLootCrate.h"
#include "World/HollowLootPickup.h"

#if !UE_BUILD_SHIPPING

namespace HollowLootConsole
{
	/** Any machine: every crate (broken or not; the server says what it holds) and every pickup on the floor. */
	void Dump(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		int32 Crates = 0;
		int32 Broken = 0;
		for (TActorIterator<AHollowLootCrate> It(World); It; ++It)
		{
			++Crates;
			Broken += It->IsBroken() ? 1 : 0;
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Loot.Dump: crate %s at %s, %s%s"), *It->GetName(), *It->GetActorLocation().ToCompactString(),
				It->IsBroken() ? TEXT("broken") : *FString::Printf(TEXT("health %.0f"), It->GetHealth()),
				World->GetNetMode() != NM_Client ? *FString::Printf(TEXT(", seed %d holds %s"), It->GetSeed(), *HollowLoadoutRules::Describe(It->GetContents())) : TEXT(""));
		}
		int32 Pickups = 0;
		for (TActorIterator<AHollowLootPickup> It(World); It; ++It)
		{
			++Pickups;
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Loot.Dump: pickup %s %d at %s%s"), *GetNameSafe(It->GetItem()), It->GetCount(),
				*It->GetActorLocation().ToCompactString(), It->IsTaken() ? TEXT(" (taken)") : TEXT(""));
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Loot.Dump: %d crate(s), %d broken, %d pickup(s)"), Crates, Broken, Pickups);
	}

	/** The local player looks at the nearest unbroken crate (Crates) or untaken pickup. */
	template <typename TActor>
	void FaceNearest(UWorld* World, TFunctionRef<bool(const TActor&)> Wanted, const TCHAR* Command)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		const TActor* Nearest = nullptr;
		for (TActorIterator<TActor> It(World); Pawn && It; ++It)
		{
			if (Wanted(**It) && (!Nearest || FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation())
				< FVector::DistSquared(Nearest->GetActorLocation(), Pawn->GetActorLocation())))
			{
				Nearest = *It;
			}
		}
		if (!Nearest)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("%s: no local pawn or nothing to face."), Command);
			return;
		}
		FVector Eyes;
		FRotator Unused;
		Pawn->GetActorEyesViewPoint(Eyes, Unused);
		Player->SetControlRotation((Nearest->GetActorLocation() - Eyes).Rotation());
		UE_LOG(LogHollowGame, Display, TEXT("%s: faces %s %.0f cm away"), Command, *Nearest->GetName(), FVector::Dist(Eyes, Nearest->GetActorLocation()));
	}

	void FaceNearestCrate(UWorld* World)
	{
		FaceNearest<AHollowLootCrate>(World, [](const AHollowLootCrate& Crate) { return !Crate.IsBroken(); }, TEXT("Hollow.Dev.FaceNearestCrate"));
	}

	void FaceNearestPickup(UWorld* World)
	{
		FaceNearest<AHollowLootPickup>(World, [](const AHollowLootPickup& Pickup) { return !Pickup.IsTaken(); }, TEXT("Hollow.Dev.FaceNearestPickup"));
	}

	FAutoConsoleCommandWithWorld FaceNearestCrateCommand(
		TEXT("Hollow.Dev.FaceNearestCrate"),
		TEXT("Turns the local player towards the nearest unbroken loot crate."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&FaceNearestCrate));

	FAutoConsoleCommandWithWorld FaceNearestPickupCommand(
		TEXT("Hollow.Dev.FaceNearestPickup"),
		TEXT("Turns the local player towards the nearest loot on the floor (then Hollow.Interact takes it)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&FaceNearestPickup));

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Loot.Dump"),
		TEXT("Prints every loot crate (its health, or broken; on the server its seed and contents) and every pickup on the floor."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
