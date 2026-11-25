// Development console commands for the crew logs (NAR-01b, ADR-0050): the datapads, this machine's archive. Never shipped.
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Lore/HollowLoreEntryDefinition.h"
#include "Lore/HollowLoreRules.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Raid/HollowRaidStateComponent.h"
#include "World/HollowDatapad.h"

#if !UE_BUILD_SHIPPING

namespace HollowLoreConsole
{
	/** Any machine: its archive (found of all, each log), the raid's downloaded logs and the datapads it sees. */
	void Dump(UWorld* World)
	{
		const UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(World);
		const TArray<const UHollowLoreEntryDefinition*> All = UHollowLoreEntryDefinition::LoadAll();
		TArray<FPrimaryAssetId> Ids;
		for (const UHollowLoreEntryDefinition* Entry : All)
		{
			Ids.Add(Entry->GetPrimaryAssetId());
		}
		const TArray<FPrimaryAssetId> Known = Profile ? Profile->GetProfile().Lore : TArray<FPrimaryAssetId>();
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Lore.Dump: archive %d/%d"), HollowLoreRules::CountKnown(Ids, Known), Ids.Num());
		for (const UHollowLoreEntryDefinition* Entry : All)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Lore.Dump: %2d %s %s \"%s\"%s"), Entry->Number, *Entry->SectorFeature, *Entry->GetName(),
				*Entry->Title.ToString(), Known.Contains(Entry->GetPrimaryAssetId()) ? TEXT(" found") : TEXT(""));
		}
		if (const UHollowRaidStateComponent* Raid = UHollowRaidStateComponent::Get(World))
		{
			TArray<FString> Names;
			for (const FPrimaryAssetId& Id : Raid->GetLoreFound())
			{
				Names.Add(Id.PrimaryAssetName.ToString());
			}
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Lore.Dump: raid downloads [%s]"), *FString::Join(Names, TEXT(", ")));
		}
		int32 Pads = 0;
		for (TActorIterator<AHollowDatapad> It(World); It; ++It)
		{
			++Pads;
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Lore.Dump: datapad %s at %s%s"), *It->GetName(), *It->GetActorLocation().ToCompactString(),
				It->IsTaken() ? TEXT(" (downloaded)") : TEXT(""));
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Lore.Dump: %d datapad(s)"), Pads);
	}

	/** This machine: adds crew logs to its archive (a number 1..12, or All). Usage: Hollow.Lore.Grant <Number|All> */
	void Grant(const TArray<FString>& Args, UWorld* World)
	{
		UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(World);
		if (!Profile || Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Lore.Grant <Number|All> needs a local profile."));
			return;
		}
		const bool bAll = Args[0].Equals(TEXT("All"), ESearchCase::IgnoreCase);
		const int32 Number = FCString::Atoi(*Args[0]);
		int32 Added = 0;
		for (const UHollowLoreEntryDefinition* Entry : UHollowLoreEntryDefinition::LoadAll())
		{
			if (bAll || Entry->Number == Number)
			{
				Added += Profile->AddLoreEntry(Entry->GetPrimaryAssetId()) ? 1 : 0;
			}
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Lore.Grant %s: %d new, %d in the archive"), *Args[0], Added, Profile->GetProfile().Lore.Num());
	}

	/** This machine: forgets every crew log found. */
	void Reset(UWorld* World)
	{
		if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(World))
		{
			Profile->ForgetLore();
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Lore.Reset: the archive is empty"));
		}
	}

	/** The local player looks at the nearest datapad not downloaded (then Hollow.Interact downloads it). */
	void FaceNearestDatapad(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		const AHollowDatapad* Nearest = nullptr;
		for (TActorIterator<AHollowDatapad> It(World); Pawn && It; ++It)
		{
			if (!It->IsTaken() && (!Nearest || FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation())
				< FVector::DistSquared(Nearest->GetActorLocation(), Pawn->GetActorLocation())))
			{
				Nearest = *It;
			}
		}
		if (!Nearest)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.FaceNearestDatapad: no local pawn or no datapad left."));
			return;
		}
		FVector Eyes;
		FRotator Unused;
		Pawn->GetActorEyesViewPoint(Eyes, Unused);
		Player->SetControlRotation((Nearest->GetActorLocation() - Eyes).Rotation());
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.FaceNearestDatapad: faces %s %.0f cm away"), *Nearest->GetName(), FVector::Dist(Eyes, Nearest->GetActorLocation()));
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Lore.Dump"),
		TEXT("Prints this machine's crew log archive, the raid's downloads and the datapads it sees."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));

	FAutoConsoleCommandWithWorldAndArgs GrantCommand(
		TEXT("Hollow.Lore.Grant"),
		TEXT("Adds crew logs to this machine's archive. Usage: Hollow.Lore.Grant <Number 1..12|All>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Grant));

	FAutoConsoleCommandWithWorld ResetCommand(
		TEXT("Hollow.Lore.Reset"),
		TEXT("Forgets every crew log of this machine's archive."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Reset));

	FAutoConsoleCommandWithWorld FaceNearestDatapadCommand(
		TEXT("Hollow.Dev.FaceNearestDatapad"),
		TEXT("Turns the local player towards the nearest crew datapad (then Hollow.Interact downloads it)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&FaceNearestDatapad));
}

#endif // !UE_BUILD_SHIPPING
