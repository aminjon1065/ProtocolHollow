// Development console commands for headless and multi-process testing. Never shipped.
#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowToolSet.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Data/HollowBiomeDefinition.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Data/HollowRoomDefinition.h"
#include "Assembly/HollowSectorAssemblySubsystem.h"
#include "Assembly/HollowSectorStateComponent.h"
#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "Framework/HollowRaidGameMode.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Generation/HollowSectorGenerator.h"
#include "Interfaces/HollowToolTarget.h"
#include "Tags/HollowGameplayTags.h"
#include "Generation/HollowSectorTemplates.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Interaction/HollowInteractionComponent.h"
#include "Interaction/HollowInteractionSettings.h"
#include "Sector/HollowSectorAuthorityComponent.h"
#include "Sector/HollowSectorFeatureSubsystem.h"
#include "Sector/HollowSectorSettings.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"
#include "World/HollowSectorMarker.h"

#if !UE_BUILD_SHIPPING

namespace HollowDevConsole
{
	/** Same as pressing Interact: asks the server to interact with the local player's focus. */
	void Interact(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const AHollowExoFrameCharacter* Character = Controller ? Cast<AHollowExoFrameCharacter>(Controller->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Interact: no local ExoFrame."));
			return;
		}
		FVector EyeLocation;
		FRotator EyeRotation;
		Character->GetActorEyesViewPoint(EyeLocation, EyeRotation);
		UE_LOG(LogHollowGame, Log, TEXT("Hollow.Interact: %s at %s, eyes %s looking %s, requests %s"), *Character->GetName(),
			*Character->GetActorLocation().ToCompactString(), *EyeLocation.ToCompactString(), *EyeRotation.ToCompactString(),
			*GetNameSafe(Character->GetInteractionComponent()->GetFocusedActor()));
		Character->GetInteractionComponent()->RequestInteract();
	}

	/** Runs a console command later; the timer lives on the game instance, so it survives map travel. */
	void RunAfter(const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (Args.Num() < 2 || !GameInstance)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Dev.RunAfter <seconds> <command...>"));
			return;
		}

		const float DelaySeconds = FCString::Atof(*Args[0]);
		const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
		FTimerHandle Handle;
		GameInstance->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(GameInstance, [GameInstance, Command]()
		{
			UE_LOG(LogHollowGame, Log, TEXT("Hollow.Dev.RunAfter: %s"), *Command);
			GEngine->Exec(GameInstance->GetWorld(), *Command);
		}), FMath::Max(DelaySeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
	}

	/** Hollow.Sector.* commands take the sector plugin name, e.g. HollowSector_Docks. */
	UHollowSectorFeatureSubsystem* SectorsFor(const TArray<FString>& Args, const TCHAR* Command)
	{
		UHollowSectorFeatureSubsystem* Sectors = UHollowSectorFeatureSubsystem::Get();
		if (Args.Num() != 1 || !Sectors)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: %s <SectorPlugin>, e.g. %s HollowSector_Docks"), Command, Command);
			return nullptr;
		}
		return Sectors;
	}

	void SectorActivate(const TArray<FString>& Args)
	{
		if (UHollowSectorFeatureSubsystem* Sectors = SectorsFor(Args, TEXT("Hollow.Sector.Activate")))
		{
			Sectors->ActivateSector(Args[0], {});
		}
	}

	void SectorDeactivate(const TArray<FString>& Args)
	{
		if (UHollowSectorFeatureSubsystem* Sectors = SectorsFor(Args, TEXT("Hollow.Sector.Deactivate")))
		{
			Sectors->DeactivateSector(Args[0], {});
		}
	}

	void SectorStatus(const TArray<FString>& Args)
	{
		if (const UHollowSectorFeatureSubsystem* Sectors = SectorsFor(Args, TEXT("Hollow.Sector.Status")))
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Sector.Status: %s"), *Sectors->DescribeSector(Args[0]));
		}
	}

	/** Generates a sector from the plugin's biome rules and rooms and prints it (dev only: loads synchronously). */
	void PcgGenerate(const TArray<FString>& Args)
	{
		if (Args.Num() != 2 || !Args[1].IsNumeric())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.PCG.Generate <SectorPlugin> <Seed>, e.g. Hollow.PCG.Generate HollowSector_Docks 42"));
			return;
		}
		const FString& Plugin = Args[0];
		const FPrimaryAssetId BiomeId = UHollowSectorFeatureSubsystem::GetSectorBiome(Plugin);
		const UHollowBiomeDefinition* Biome = BiomeId.IsValid()
			? Cast<UHollowBiomeDefinition>(UAssetManager::Get().GetPrimaryAssetPath(BiomeId).TryLoad()) : nullptr;
		if (!Biome)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.PCG.Generate: %s has no biome definition."), *Plugin);
			return;
		}

		TArray<const UHollowRoomDefinition*> Rooms;
		for (const FPrimaryAssetId& Id : UHollowSectorFeatureSubsystem::GetSectorRooms(Plugin))
		{
			Rooms.Add(Cast<UHollowRoomDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad()));
		}

		FHollowSectorGenInput Input;
		Input.Seed = FCString::Atoi(*Args[1]);
		Input.Rules = Biome->GenRules;
		Input.Templates = HollowSectorTemplates::MakeTemplates(Rooms, Biome->GenRules);
		FHollowSectorLayout Layout;
		const EHollowGenResult Result = FHollowSectorGenerator::Generate(Input, Layout);
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.PCG.Generate %s seed %d: %s"), *Plugin, Input.Seed, *HollowEnumToString(Result));
		if (Result == EHollowGenResult::Success)
		{
			for (const FString& Line : FHollowSectorGenerator::RenderAscii(Layout))
			{
				UE_LOG(LogHollowGame, Display, TEXT("%s"), *Line);
			}
		}
	}

	/** Host only: seamless travel of the whole party to the raid map with a seed (until the raid flow, S3-03). */
	void RaidStart(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Raid.Start runs on the host."));
			return;
		}
		const int32 Seed = Args.Num() > 0 && Args[0].IsNumeric() ? FCString::Atoi(*Args[0]) : UHollowSectorAuthorityComponent::MakeRandomSeed();
		const FString Url = FString::Printf(TEXT("%s?Seed=%d"), *GetDefault<UHollowSectorSettings>()->RaidMap.GetLongPackageName(), Seed);
		UE_LOG(LogHollowGame, Log, TEXT("Hollow.Raid.Start: travelling to %s"), *Url);
		World->ServerTravel(Url);
	}

	/** Server only: replaces the sector with a new seed on every machine, without travel. */
	void SectorRebuild(const TArray<FString>& Args, UWorld* World)
	{
		const AHollowRaidGameMode* Raid = World ? World->GetAuthGameMode<AHollowRaidGameMode>() : nullptr;
		if (!Raid || Args.Num() != 1 || !Args[0].IsNumeric())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (host, in a raid): Hollow.Sector.Rebuild <Seed>"));
			return;
		}
		Raid->GetSectorAuthority()->StartBuild(FCString::Atoi(*Args[0]));
	}

	/** This machine's view of the sector: manifest, local assembly and replicated gameplay actors. */
	void SectorInfo(UWorld* World)
	{
		const UHollowSectorStateComponent* State = UHollowSectorStateComponent::Get(World);
		const UHollowSectorAssemblySubsystem* Assembly = World ? World->GetSubsystem<UHollowSectorAssemblySubsystem>() : nullptr;
		if (!State || !Assembly)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Sector.Info: no sector in this world."));
			return;
		}
		int32 Markers = 0;
		for (TActorIterator<AHollowSectorMarker> It(World); It; ++It)
		{
			++Markers;
		}
		const FHollowSectorManifest& Manifest = State->GetManifest();
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Sector.Info: build %d seed %d hash %08X ready %d | local: assembled %d, hash %08X, rooms %d/%d visible, plugs %d, markers %d"),
			Manifest.BuildId, Manifest.Seed, Manifest.LayoutHash, State->IsSectorReady(), Assembly->IsAssembled(), Assembly->GetLocalHash(),
			Assembly->NumVisibleRooms(), Assembly->NumRoomInstances(), Assembly->NumPlugs(), Markers);
		TMap<EHollowDoorState, int32> DoorStates;
		const AHollowDoor* NearestDoor = nullptr;
		const UGameInstance* GameInstance = World->GetGameInstance();
		const APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const FVector Here = Player && Player->GetPawn() ? Player->GetPawn()->GetActorLocation() : FVector::ZeroVector;
		for (TActorIterator<AHollowDoor> It(World); It; ++It)
		{
			++DoorStates.FindOrAdd(It->GetDoorState()->GetNetState().State);
			if (!NearestDoor || FVector::DistSquared(It->GetActorLocation(), Here) < FVector::DistSquared(NearestDoor->GetActorLocation(), Here))
			{
				NearestDoor = *It;
			}
		}
		FString Doors;
		for (const EHollowDoorState DoorState : {EHollowDoorState::Closed, EHollowDoorState::Opening, EHollowDoorState::Open, EHollowDoorState::Closing, EHollowDoorState::Welded})
		{
			Doors += FString::Printf(TEXT(" %s %d"), *HollowEnumToString(DoorState), DoorStates.FindRef(DoorState));
		}
		if (NearestDoor)
		{
			const FHollowDoorNetState& Door = NearestDoor->GetDoorState()->GetNetState();
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Sector.Info: doors%s | nearest %s %s weld %.2f (replicated %d) serial %d leaf %.2f"), *Doors,
				*NearestDoor->GetName(), *HollowEnumToString(Door.State), NearestDoor->GetDoorState()->GetWeld(), Door.WeldQ, Door.TransitionSerial, NearestDoor->GetLeafAlpha());
		}
		if (const ACharacter* Character = Player ? Cast<ACharacter>(Player->GetPawn()) : nullptr)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Sector.Info: local pawn %s at %s, on ground %d"), *Character->GetName(),
				*Character->GetActorLocation().ToCompactString(), Character->GetCharacterMovement()->IsMovingOnGround());
		}
		if (const AHollowRaidGameMode* Raid = World->GetAuthGameMode<AHollowRaidGameMode>())
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Sector.Info: %s"), *Raid->GetSectorAuthority()->Describe());
		}
	}

	UHollowAbilitySystemComponent* LocalAbilitySystem(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const AHollowExoFrameCharacter* Character = Player ? Cast<AHollowExoFrameCharacter>(Player->GetPawn()) : nullptr;
		return Character ? Cast<UHollowAbilitySystemComponent>(Character->GetAbilitySystemComponent()) : nullptr;
	}

	/** Holds an ability input for a while through the same path as the keyboard (S2-03 tests). */
	void ToolHold(const TArray<FString>& Args, UWorld* World)
	{
		const FGameplayTag InputTag = Args.Num() == 2 ? FGameplayTag::RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound=*/false) : FGameplayTag();
		UHollowAbilitySystemComponent* AbilitySystem = LocalAbilitySystem(World);
		if (!InputTag.IsValid() || !Args[1].IsNumeric() || !AbilitySystem)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (with a local ExoFrame): Hollow.Tool.Hold <Hollow.Input.Tag> <Seconds>"));
			return;
		}
		UE_LOG(LogHollowGame, Log, TEXT("Hollow.Tool.Hold: %s for %s s"), *InputTag.ToString(), *Args[1]);
		AbilitySystem->AbilityInputTagPressed(InputTag);
		FTimerHandle Handle;
		World->GetGameInstance()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(AbilitySystem, [AbilitySystem, InputTag]()
		{
			AbilitySystem->AbilityInputTagReleased(InputTag);
		}), FMath::Max(FCString::Atof(*Args[1]), UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
	}

	/** Turns the local player towards the nearest tool target. */
	void FaceToolTarget(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.FaceToolTarget: no local pawn."));
			return;
		}
		FVector Eyes;
		FRotator Unused;
		Pawn->GetActorEyesViewPoint(Eyes, Unused);
		const AActor* Nearest = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Implements<UHollowToolTarget>() && (!Nearest || FVector::DistSquared(It->GetActorLocation(), Eyes) < FVector::DistSquared(Nearest->GetActorLocation(), Eyes)))
			{
				Nearest = *It;
			}
		}
		if (Nearest)
		{
			FVector Centre;
			FVector Extent;
			Nearest->GetActorBounds(/*bOnlyCollidingComponents=*/true, Centre, Extent);
			Player->SetControlRotation((Centre - Eyes).Rotation());
			UE_LOG(LogHollowGame, Log, TEXT("Hollow.Dev.FaceToolTarget: %s"), *Nearest->GetName());
		}
	}

	/** Host only: puts the local pawn in front of the nearest door, facing it (S2-04 tests). */
	void GoToNearestDoor(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		ACharacter* Character = Player ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
		if (!Character || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.GoToNearestDoor runs on the host with a local pawn."));
			return;
		}
		const AHollowDoor* Nearest = nullptr;
		for (TActorIterator<AHollowDoor> It(World); It; ++It)
		{
			if (!Nearest || FVector::DistSquared(It->GetActorLocation(), Character->GetActorLocation()) < FVector::DistSquared(Nearest->GetActorLocation(), Character->GetActorLocation()))
			{
				Nearest = *It;
			}
		}
		if (!Nearest)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.GoToNearestDoor: no door."));
			return;
		}
		// Stay on the pawn's side of the door, well inside interaction and tool reach.
		const FVector Across = Nearest->GetActorForwardVector();
		const double Side = FVector::DotProduct(Character->GetActorLocation() - Nearest->GetActorLocation(), Across) >= 0.0 ? 1.0 : -1.0;
		const float Distance = GetDefault<UHollowInteractionSettings>()->InteractionRange * 0.5f;
		const FVector Stand = Nearest->GetActorLocation() + Across * Side * Distance + FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Character->SetActorLocation(Stand, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		FVector Centre;
		FVector Extent;
		Nearest->GetActorBounds(/*bOnlyCollidingComponents=*/true, Centre, Extent);
		FVector Eyes;
		FRotator Unused;
		Character->GetActorEyesViewPoint(Eyes, Unused);
		Player->SetControlRotation((Centre - Eyes).Rotation());
		UE_LOG(LogHollowGame, Log, TEXT("Hollow.Dev.GoToNearestDoor: %s"), *Nearest->GetName());
	}

	void ToolInfo(UWorld* World)
	{
		const UHollowAbilitySystemComponent* AbilitySystem = LocalAbilitySystem(World);
		if (!AbilitySystem)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Tool.Info: no local ExoFrame."));
			return;
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Tool.Info: fuel %.1f/%.1f, heat %.1f/%.1f, overheated %d"),
			AbilitySystem->GetNumericAttribute(UHollowToolSet::GetToolFuelAttribute()), AbilitySystem->GetNumericAttribute(UHollowToolSet::GetMaxToolFuelAttribute()),
			AbilitySystem->GetNumericAttribute(UHollowToolSet::GetToolHeatAttribute()), AbilitySystem->GetNumericAttribute(UHollowToolSet::GetMaxToolHeatAttribute()),
			AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Tool_Overheated));
	}

	FAutoConsoleCommandWithWorld InteractCommand(
		TEXT("Hollow.Interact"),
		TEXT("Interacts with the local player's current focus (same as the Interact input)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Interact));

	FAutoConsoleCommandWithWorldAndArgs RunAfterCommand(
		TEXT("Hollow.Dev.RunAfter"),
		TEXT("Runs a console command after a delay that survives map travel. Usage: Hollow.Dev.RunAfter <seconds> <command...>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunAfter));

	FAutoConsoleCommand SectorActivateCommand(
		TEXT("Hollow.Sector.Activate"),
		TEXT("Activates a sector Game Feature plugin on this machine (ADR-0012). Usage: Hollow.Sector.Activate <SectorPlugin>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SectorActivate));

	FAutoConsoleCommand SectorDeactivateCommand(
		TEXT("Hollow.Sector.Deactivate"),
		TEXT("Returns a sector plugin to Registered on this machine. Usage: Hollow.Sector.Deactivate <SectorPlugin>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SectorDeactivate));

	FAutoConsoleCommand SectorStatusCommand(
		TEXT("Hollow.Sector.Status"),
		TEXT("Prints the plugin state, references and room count of a sector. Usage: Hollow.Sector.Status <SectorPlugin>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&SectorStatus));

	FAutoConsoleCommandWithWorldAndArgs RaidStartCommand(
		TEXT("Hollow.Raid.Start"),
		TEXT("Host: seamless travel of the party to HollowSectorSettings.RaidMap. Usage: Hollow.Raid.Start [Seed]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RaidStart));

	FAutoConsoleCommandWithWorldAndArgs SectorRebuildCommand(
		TEXT("Hollow.Sector.Rebuild"),
		TEXT("Host, in a raid: replaces the sector with a new seed on every machine. Usage: Hollow.Sector.Rebuild <Seed>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SectorRebuild));

	FAutoConsoleCommandWithWorld SectorInfoCommand(
		TEXT("Hollow.Sector.Info"),
		TEXT("Prints this machine's sector: manifest, local assembly, gameplay actors (and the authority on the host)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&SectorInfo));

	FAutoConsoleCommandWithWorldAndArgs ToolHoldCommand(
		TEXT("Hollow.Tool.Hold"),
		TEXT("Holds an ability input like the keyboard would. Usage: Hollow.Tool.Hold <Hollow.Input.Tag> <Seconds>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ToolHold));

	FAutoConsoleCommandWithWorld FaceToolTargetCommand(
		TEXT("Hollow.Dev.FaceToolTarget"),
		TEXT("Turns the local player towards the nearest tool target."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&FaceToolTarget));

	FAutoConsoleCommandWithWorld GoToNearestDoorCommand(
		TEXT("Hollow.Dev.GoToNearestDoor"),
		TEXT("Host: puts the local pawn in front of the nearest door, facing it."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&GoToNearestDoor));

	FAutoConsoleCommandWithWorld ToolInfoCommand(
		TEXT("Hollow.Tool.Info"),
		TEXT("Prints the local player's tool fuel, heat and overheat state."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ToolInfo));

	FAutoConsoleCommand PcgGenerateCommand(
		TEXT("Hollow.PCG.Generate"),
		TEXT("Generates a sector layout from a sector plugin's biome rules and prints an ASCII map. Usage: Hollow.PCG.Generate <SectorPlugin> <Seed>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&PcgGenerate));
}

#endif // !UE_BUILD_SHIPPING
