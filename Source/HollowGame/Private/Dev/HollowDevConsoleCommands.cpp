// Development console commands for headless and multi-process testing. Never shipped.
#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowToolSet.h"
#include "Breach/HollowBreachableBulkhead.h"
#include "Attributes/HollowHealthSet.h"
#include "Carry/HollowAetherCore.h"
#include "GameplayEffect.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Raid/HollowExtractionZone.h"
#include "Raid/HollowRaidFlowComponent.h"
#include "Raid/HollowRaidSettings.h"
#include "Raid/HollowRaidStateComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Data/HollowBiomeDefinition.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Data/HollowRoomDefinition.h"
#include "Assembly/HollowSectorAssemblySubsystem.h"
#include "Assembly/HollowSectorStateComponent.h"
#include "Engine/AssetManager.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "EngineUtils.h"
#include "Threat/HollowThreatDirectorSubsystem.h"
#include "Framework/HollowRaidGameMode.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Generation/HollowSectorGenerator.h"
#include "Interfaces/HollowStructural.h"
#include "Interfaces/HollowToolTarget.h"
#include "Nav/HollowNavSettings.h"
#include "Nav/HollowNavUpdateSubsystem.h"
#include "NavAreas/NavArea.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavLinkCustomComponent.h"
#include "Tags/HollowGameplayTags.h"
#include "Generation/HollowSectorTemplates.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Interaction/HollowInteractionComponent.h"
#include "Interaction/HollowInteractionSettings.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/HollowResonanceSet.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Resonance/HollowResonancePresentationSubsystem.h"
#include "Resonance/HollowSquadResonanceComponent.h"
#include "Algo/Count.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Rules/HollowLightBudget.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Sector/HollowSectorAuthorityComponent.h"
#include "Sector/HollowSectorFeatureSubsystem.h"
#include "Sector/HollowSectorSettings.h"
#include "TimerManager.h"
#include "Util/HollowEnumUtils.h"
#include "World/HollowSectorMarker.h"

#if !UE_BUILD_SHIPPING

namespace HollowDevConsolePrivate
{
	/** Gameplay ability system levels start at 1; development effects have no levels. */
	constexpr float EffectLevel = 1.f;
}

namespace HollowDevConsole
{
	/** Same as pressing Interact: asks the server to interact with the local player's focus. */
	void Interact(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		AHollowExoFrameCharacter* Character = Controller ? Cast<AHollowExoFrameCharacter>(Controller->GetPawn()) : nullptr;
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
		Character->RequestInteract();   // the input's path: drops a carried core first
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

	/** How often Hollow.Dev.RunInRaid checks whether this machine is in a ready raid. */
	constexpr float RaidReadyPollSeconds = 0.25f;

	/**
	 * Runs a console command a delay after this machine is in a ready raid (sector ready and its own pawn spawned), so
	 * scenarios of several processes share one clock whatever their boot and travel times. Survives map travel.
	 * Usage: Hollow.Dev.RunInRaid <seconds> <command...>
	 */
	void RunInRaid(const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (Args.Num() < 2 || !GameInstance)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Dev.RunInRaid <seconds> <command...>"));
			return;
		}
		const float DelaySeconds = FCString::Atof(*Args[0]);
		const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
		const TSharedRef<FTimerHandle> Poll = MakeShared<FTimerHandle>();
		GameInstance->GetTimerManager().SetTimer(*Poll, FTimerDelegate::CreateWeakLambda(GameInstance, [GameInstance, Command, DelaySeconds, Poll]()
		{
			UWorld* Current = GameInstance->GetWorld();
			const UHollowSectorStateComponent* Sector = UHollowSectorStateComponent::Get(Current);
			const APlayerController* Player = GameInstance->GetFirstLocalPlayerController(Current);
			if (!Sector || !Sector->IsSectorReady() || !Player || !Player->GetPawn())
			{
				return;
			}
			FTimerHandle Run;
			GameInstance->GetTimerManager().SetTimer(Run, FTimerDelegate::CreateWeakLambda(GameInstance, [GameInstance, Command]()
			{
				UE_LOG(LogHollowGame, Log, TEXT("Hollow.Dev.RunInRaid: %s"), *Command);
				GEngine->Exec(GameInstance->GetWorld(), *Command);
			}), FMath::Max(DelaySeconds, UE_KINDA_SMALL_NUMBER), /*bLoop=*/false);
			// Last: clearing destroys this very delegate and its captures.
			GameInstance->GetTimerManager().ClearTimer(*Poll);
		}), RaidReadyPollSeconds, /*bLoop=*/true);
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

	/** Directions tried round a target when the spot in front of it is taken (every 45 degrees). */
	constexpr int32 PlaceDirections = 8;
	/** The free-spot probe is this much smaller than the capsule: standing on the floor is not being inside it. */
	constexpr float PlaceProbeShrink = 2.f;

	/**
	 * Host: puts Player's pawn in front of its nearest actor of Class (a door or a bulkhead, both face X across their
	 * opening; a core), facing it, Offset uu sideways along it. If the capsule does not fit there, or a wall stands
	 * between the target and the spot (a core in a corner), the same spot turned round the target, front first.
	 */
	void PlaceAtNearest(APlayerController& Player, const double Offset, const TSubclassOf<AActor> Class, const TCHAR* Command)
	{
		ACharacter* Character = Cast<ACharacter>(Player.GetPawn());
		UWorld* World = Player.GetWorld();
		const AActor* Nearest = nullptr;
		for (TActorIterator<AActor> It(World, Class); It && Character; ++It)
		{
			if (!Nearest || FVector::DistSquared(It->GetActorLocation(), Character->GetActorLocation()) < FVector::DistSquared(Nearest->GetActorLocation(), Character->GetActorLocation()))
			{
				Nearest = *It;
			}
		}
		if (!Nearest)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("%s: no pawn or no target for %s."), Command, *Player.GetName());
			return;
		}
		// Stay on the pawn's side of the target, well inside interaction and tool reach, on the floor it stands on (the
		// bottom of its collision: a door's pivot is on the floor, a core's in its middle).
		FVector Centre;
		FVector Extent;
		Nearest->GetActorBounds(/*bOnlyCollidingComponents=*/true, Centre, Extent);
		if (Extent.IsNearlyZero())
		{
			Nearest->GetActorBounds(/*bOnlyCollidingComponents=*/false, Centre, Extent);   // a volume without collision (extraction zone)
		}
		const FVector Across = Nearest->GetActorForwardVector();
		const double Side = FVector::DotProduct(Character->GetActorLocation() - Nearest->GetActorLocation(), Across) >= 0.0 ? 1.0 : -1.0;
		const float Distance = GetDefault<UHollowInteractionSettings>()->InteractionRange * 0.5f;
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const double StandZ = Centre.Z - Extent.Z + Capsule->GetScaledCapsuleHalfHeight();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowDevPlace), /*bTraceComplex=*/false, Character);
		Params.AddIgnoredActor(Nearest);
		FCollisionResponseParams Responses;
		Capsule->InitSweepCollisionParams(Params, Responses);
		auto SpotAt = [&](const FVector& Direction)
		{
			FVector Spot = Nearest->GetActorLocation() + Direction * Distance + FVector::CrossProduct(FVector::UpVector, Direction) * Offset;
			Spot.Z = StandZ;
			return Spot;
		};
		FVector Stand = SpotAt(Across * Side);
		for (int32 Step = 0; Step < PlaceDirections; ++Step)
		{
			const int32 Turn = (Step + 1) / 2 * (Step % 2 == 1 ? 1 : -1);
			const FVector Spot = SpotAt((Across * Side).RotateAngleAxis(Turn * 360.0 / PlaceDirections, FVector::UpVector));
			FHitResult Hit;
			const bool bInside = World->OverlapBlockingTestByChannel(Spot, FQuat::Identity, Capsule->GetCollisionObjectType(),
				Capsule->GetCollisionShape(-PlaceProbeShrink), Params, Responses);
			const bool bWall = !bInside && World->LineTraceSingleByChannel(Hit, FVector(Centre.X, Centre.Y, StandZ), Spot, Capsule->GetCollisionObjectType(), Params, Responses);
			UE_LOG(LogHollowGame, Verbose, TEXT("%s: %s at %s: inside %d, wall %s"), Command, *Player.GetName(), *Spot.ToCompactString(), bInside,
				bWall ? *GetNameSafe(Hit.GetActor()) : TEXT("none"));
			if (!bInside && !bWall)
			{
				Stand = Spot;
				break;
			}
		}
		Character->SetActorLocation(Stand, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		FVector Eyes;
		FRotator Unused;
		Character->GetActorEyesViewPoint(Eyes, Unused);
		const FRotator Facing = (Centre - Eyes).Rotation();
		if (Player.IsLocalController())
		{
			Player.SetControlRotation(Facing);
		}
		else
		{
			Player.ClientSetRotation(Facing);   // a remote player's view is theirs: ask their machine
		}
		UE_LOG(LogHollowGame, Log, TEXT("%s: %s at %s"), Command, *Player.GetName(), *Nearest->GetName());
	}

	/**
	 * Host: puts the local pawn (or, with All, every player's pawn, side by side; with Others, the remote players') in
	 * front of the nearest actor of Class, facing it (S2-04/S2-06/S3-01/S4-01 tests).
	 */
	void GoToNearest(const TArray<FString>& Args, UWorld* World, const TSubclassOf<AActor> Class, const TCHAR* Command)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("%s runs on the host."), Command);
			return;
		}
		const bool bAll = Args.Num() > 0 && Args[0].Equals(TEXT("All"), ESearchCase::IgnoreCase);
		const bool bOthers = Args.Num() > 0 && Args[0].Equals(TEXT("Others"), ESearchCase::IgnoreCase);
		TArray<APlayerController*> Players;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* Player = It->Get(); Player && (bAll || Player->IsLocalController() != bOthers))
			{
				Players.Add(Player);
			}
		}
		for (int32 Index = 0; Index < Players.Num(); ++Index)
		{
			const ACharacter* Character = Cast<ACharacter>(Players[Index]->GetPawn());
			const double Spacing = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() * 3.0 : 0.0;   // capsules never touch
			PlaceAtNearest(*Players[Index], (Index - (Players.Num() - 1) * 0.5) * Spacing, Class, Command);
		}
	}

	/** Usage: Hollow.Dev.GoToNearestDoor [All] */
	void GoToNearestDoor(const TArray<FString>& Args, UWorld* World)
	{
		GoToNearest(Args, World, AHollowDoor::StaticClass(), TEXT("Hollow.Dev.GoToNearestDoor"));
	}

	/** Usage: Hollow.Dev.GoToNearestBulkhead [All] */
	void GoToNearestBulkhead(const TArray<FString>& Args, UWorld* World)
	{
		GoToNearest(Args, World, AHollowBreachableBulkhead::StaticClass(), TEXT("Hollow.Dev.GoToNearestBulkhead"));
	}

	/** Usage: Hollow.Dev.GoToNearestEnemy [All|Others] [Stalker] */
	void GoToNearestEnemy(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const Command = TEXT("Hollow.Dev.GoToNearestEnemy");
		TArray<FString> Who = Args;
		TSubclassOf<AActor> Class = AHollowEnemyCharacter::StaticClass();
		if (Who.RemoveAll([](const FString& Arg) { return Arg.Equals(TEXT("Stalker"), ESearchCase::IgnoreCase); }) > 0)
		{
			// Only the director's stalker (any other enemy may be nearer).
			const UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(World);
			const AActor* Stalker = Director ? Director->GetStalker() : nullptr;
			if (!Stalker)
			{
				UE_LOG(LogHollowGame, Warning, TEXT("%s: there is no stalker."), Command);
				return;
			}
			Class = Stalker->GetClass();
		}
		GoToNearest(Who, World, Class, Command);
	}

	/** Usage: Hollow.Dev.GoToExtraction [All] */
	void GoToExtraction(const TArray<FString>& Args, UWorld* World)
	{
		GoToNearest(Args, World, AHollowExtractionZone::StaticClass(), TEXT("Hollow.Dev.GoToExtraction"));
	}

	/**
	 * Host: kills the local pawn (every player's with All, every remote player's with Others) through the ability system:
	 * one instant damage effect (UHollowRaidSettings::DevDamageEffect) of the target's max health.
	 * Usage: Hollow.Dev.Kill [All|Others]
	 */
	void Kill(const TArray<FString>& Args, UWorld* World)
	{
		const TSubclassOf<UGameplayEffect> Damage = GetDefault<UHollowRaidSettings>()->DevDamageEffect.LoadSynchronous();
		if (!World || World->GetNetMode() == NM_Client || !Damage)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.Kill runs on the host and needs HollowRaidSettings.DevDamageEffect."));
			return;
		}
		const bool bAll = Args.Num() > 0 && Args[0].Equals(TEXT("All"), ESearchCase::IgnoreCase);
		const bool bOthers = Args.Num() > 0 && Args[0].Equals(TEXT("Others"), ESearchCase::IgnoreCase);
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Player = It->Get();
			const AHollowExoFrameCharacter* Character = Player ? Cast<AHollowExoFrameCharacter>(Player->GetPawn()) : nullptr;
			UAbilitySystemComponent* AbilitySystem = Character ? Character->GetAbilitySystemComponent() : nullptr;
			const bool bTarget = bAll || (bOthers ? !Player->IsLocalController() : Player->IsLocalController());
			if (!AbilitySystem || !bTarget)
			{
				continue;
			}
			const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Damage, HollowDevConsolePrivate::EffectLevel, AbilitySystem->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_Damage, AbilitySystem->GetNumericAttribute(UHollowHealthSet::GetMaxHealthAttribute()));
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			UE_LOG(LogHollowGame, Log, TEXT("Hollow.Dev.Kill: %s"), *Player->GetName());
		}
	}

	/** Any machine: every streamed room's lights against its type's budget (DA_LightBudget, ADR-0021). */
	void LightDump(UWorld* World)
	{
		const UHollowSectorAssemblySubsystem* Assembly = World ? World->GetSubsystem<UHollowSectorAssemblySubsystem>() : nullptr;
		if (!Assembly || !Assembly->IsAssembled())
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Light.Dump: no assembled sector."));
			return;
		}
		const FHollowSectorManifest& Manifest = Assembly->GetManifest();
		int32 Failing = 0;
		for (int32 Index = 0; Index < Manifest.Rooms.Num(); ++Index)
		{
			const UHollowRoomDefinition* Room = Manifest.Rooms[Index].Room;
			const ULevelStreamingDynamic* Streamed = Assembly->GetRoomLevel(Index);
			const ULevel* Level = Streamed ? Streamed->GetLoadedLevel() : nullptr;
			if (!Room || !Level)
			{
				continue;
			}
			const TArray<FHollowLightInfo> Lights = HollowLightBudget::CollectLevel(*Level);
			const int32 Shadowed = Algo::CountIf(Lights, [](const FHollowLightInfo& L) { return L.bShadowed; });
			const int32 Functions = Algo::CountIf(Lights, [](const FHollowLightInfo& L) { return L.bLightFunction; });
			const int32 Volumetric = Algo::CountIf(Lights, [](const FHollowLightInfo& L) { return L.bVolumetricShadow; });
			const TArray<FString> Problems = HollowLightBudget::CheckRoomLevel(*Room, *Level);
			Failing += Problems.IsEmpty() ? 0 : 1;
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Light.Dump: room %d %s: %d lights, %d shadowed, %d light functions, %d volumetric shadows%s%s"),
				Index, *Room->GetName(), Lights.Num(), Shadowed, Functions, Volumetric, Problems.IsEmpty() ? TEXT(", within budget") : TEXT(" - OVER: "),
				*FString::Join(Problems, TEXT("; ")));
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Light.Dump: %d of %d rooms over budget"), Failing, Manifest.Rooms.Num());
		int32 Fogs = 0;
		int32 VolumetricFogs = 0;
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			++Fogs;
			VolumetricFogs += It->GetComponent()->bEnableVolumetricFog ? 1 : 0;
		}
		int32 Unbound = 0;
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			Unbound += It->bUnbound ? 1 : 0;
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Light.Dump: atmosphere %d height fog(s), %d volumetric, %d unbound post-process volume(s)"), Fogs, VolumetricFogs, Unbound);
	}

	/** Usage: Hollow.Resonance.Add <Amount> [All|Others] */
	void ResonanceAdd(const TArray<FString>& Args, UWorld* World)
	{
		const TSubclassOf<UGameplayEffect> Effect = GetDefault<UHollowRaidSettings>()->DevResonanceEffect.LoadSynchronous();
		if (!World || World->GetNetMode() == NM_Client || !Effect || Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Resonance.Add <Amount> [All|Others] runs on the host and needs HollowRaidSettings.DevResonanceEffect."));
			return;
		}
		const float Amount = FCString::Atof(*Args[0]);
		const bool bAll = Args.Num() > 1 && Args[1].Equals(TEXT("All"), ESearchCase::IgnoreCase);
		const bool bOthers = Args.Num() > 1 && Args[1].Equals(TEXT("Others"), ESearchCase::IgnoreCase);
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Player = It->Get();
			UAbilitySystemComponent* AbilitySystem = Player ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player->PlayerState) : nullptr;
			if (!AbilitySystem || !(bAll || (bOthers ? !Player->IsLocalController() : Player->IsLocalController())))
			{
				continue;
			}
			// Through an effect like any gain: the attribute set clamps it to 0..MaxResonance.
			const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Effect, HollowDevConsolePrivate::EffectLevel, AbilitySystem->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_ResonanceGain, Amount);
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			UE_LOG(LogHollowGame, Log, TEXT("Hollow.Resonance.Add: %s %+.1f -> %.1f"), *Player->GetName(), Amount,
				AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetResonanceAttribute()));
		}
	}

	/** Any machine: the squad's replicated Resonance, every player's, and what this machine shows of it. */
	void ResonanceDump(UWorld* World)
	{
		const UHollowSquadResonanceComponent* Squad = UHollowSquadResonanceComponent::Get(World);
		if (!Squad)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Resonance.Dump: no squad Resonance."));
			return;
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Resonance.Dump: squad %s"), *Squad->Describe());
		for (const APlayerState* Player : World->GetGameState()->PlayerArray)
		{
			const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
			if (AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(UHollowResonanceSet::GetResonanceAttribute()))
			{
				UE_LOG(LogHollowGame, Display, TEXT("Hollow.Resonance.Dump: player %s %.1f / %.1f, gain x%.2f, dead %d"), *Player->GetPlayerName(),
					AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetResonanceAttribute()),
					AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetMaxResonanceAttribute()),
					AbilitySystem->GetNumericAttribute(UHollowResonanceSet::GetResonanceGainMultiplierAttribute()),
					AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Dead));
			}
		}
		if (const UHollowResonancePresentationSubsystem* Presentation = UHollowResonancePresentationSubsystem::Get(World))
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Resonance.Dump: shown here %s"), *Presentation->Describe());
		}
	}

	/** This machine's view of the raid (phase, timer, zone) and, on the host, every player's outcome. */
	void RaidDump(UWorld* World)
	{
		const UHollowRaidStateComponent* Raid = UHollowRaidStateComponent::Get(World);
		if (!Raid)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Raid.Dump: no raid state."));
			return;
		}
		const FHollowRaidNetState& State = Raid->GetState();
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Raid.Dump: raid %s %s, %.1f s left, %d alive, %d in the zone, %d core(s) in the zone"),
			*State.RaidId.ToString(), *HollowEnumToString(State.Phase), Raid->GetSecondsLeft(), State.LivingPlayers, State.LivingInZone, State.CoresInZone);
		if (const AHollowRaidGameMode* GameMode = World->GetAuthGameMode<AHollowRaidGameMode>())
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Raid.Dump: %s"), *GameMode->GetRaidFlow()->Describe());
		}
	}

	FString DescribeStacks(const TArray<FHollowItemStack>& Stacks)
	{
		FString Text;
		for (const FHollowItemStack& Stack : Stacks)
		{
			Text += FString::Printf(TEXT(" %s x%d"), *Stack.Item.PrimaryAssetName.ToString(), Stack.Count);
		}
		return Text.IsEmpty() ? FString(TEXT(" -")) : Text;
	}

	/** This machine's local profile: credits, loadout, escrow, the debrief not seen yet, counters. */
	void ProfileDump(UWorld* World)
	{
		const UHollowProfileSubsystem* Profiles = UHollowProfileSubsystem::Get(World);
		if (!Profiles)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Profile.Dump: no profile."));
			return;
		}
		const FHollowProfile& Profile = Profiles->GetProfile();
		FString Loadout;
		for (int32 Index = 0; Index < Profile.Loadout.Num(); ++Index)
		{
			const FHollowLoadoutSlot& Slot = Profile.Loadout[Index];
			Loadout += FString::Printf(TEXT(" [%d] %s x%d%s"), Index, *Slot.Item.PrimaryAssetName.ToString(), Slot.Count, Slot.bInsured ? TEXT(" insured") : TEXT(""));
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Profile.Dump: %d credits, loadout:%s | escrow %s | raids %d, extractions %d, deaths %d, cores %d"),
			Profile.Credits, Loadout.IsEmpty() ? TEXT(" -") : *Loadout, Profiles->GetEscrow().IsActive() ? *Profiles->GetEscrow().RaidId.ToString() : TEXT("none"),
			Profile.Stats.Raids, Profile.Stats.Extractions, Profile.Stats.Deaths, Profile.Stats.CoresDelivered);
		if (const FHollowRaidReport* Report = Profiles->GetPendingReport())
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Profile.Dump: debrief pending: %s, credits %d -> %d, kept%s, lost%s, sold%s"),
				*HollowEnumToString(Report->Result.Outcome), Report->CreditsBefore, Report->CreditsAfter, *DescribeStacks(Report->Delta.Kept),
				*DescribeStacks(Report->Delta.Lost), *DescribeStacks(Report->Delta.Sold));
		}
	}

	/** Usage: Hollow.Profile.Insure <Slot> [Off] */
	void ProfileInsure(const TArray<FString>& Args, UWorld* World)
	{
		UHollowProfileSubsystem* Profiles = UHollowProfileSubsystem::Get(World);
		if (!Profiles || Args.Num() < 1 || !Args[0].IsNumeric())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Profile.Insure <Slot> [Off]"));
			return;
		}
		const bool bInsured = !(Args.Num() > 1 && Args[1].Equals(TEXT("Off"), ESearchCase::IgnoreCase));
		const int32 Slot = FCString::Atoi(*Args[0]);
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Profile.Insure: slot %d %s: %s"), Slot, bInsured ? TEXT("insured") : TEXT("uninsured"),
			Profiles->SetSlotInsured(Slot, bInsured) ? TEXT("done") : TEXT("refused (no such slot or not insurable)"));
	}

	void ProfileReset(UWorld* World)
	{
		if (UHollowProfileSubsystem* Profiles = UHollowProfileSubsystem::Get(World))
		{
			Profiles->ResetProfile();
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Profile.Reset: new profile"));
		}
	}

	/** Usage: Hollow.Dev.GoToNearestCore [All] */
	void GoToNearestCore(const TArray<FString>& Args, UWorld* World)
	{
		GoToNearest(Args, World, AHollowAetherCore::StaticClass(), TEXT("Hollow.Dev.GoToNearestCore"));
	}

	/**
	 * Host: one structural blow from the local pawn to the nearest object that still takes structural damage (S3-01,
	 * until the Iron Pilgrim does it). It lands on the part nearest to the pawn; without Amount it breaks that part.
	 * FromBehind: the blow comes from the far side, so debris flies at the pawn (the Pilgrim breaking through).
	 * Usage: Hollow.Dev.Breach [Amount] [FromBehind]
	 */
	void Breach(const TArray<FString>& Args, UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (!Pawn || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.Breach runs on the host with a local pawn."));
			return;
		}
		AActor* Nearest = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const IHollowStructural* Structural = Cast<IHollowStructural>(*It);
			if (Structural && Structural->CanTakeStructuralDamage()
				&& (!Nearest || FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) < FVector::DistSquared(Nearest->GetActorLocation(), Pawn->GetActorLocation())))
			{
				Nearest = *It;
			}
		}
		if (!Nearest)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.Breach: nothing left to break."));
			return;
		}
		const bool bFromBehind = Args.ContainsByPredicate([](const FString& Arg) { return Arg.Equals(TEXT("FromBehind"), ESearchCase::IgnoreCase); });
		const FString* AmountArg = Args.FindByPredicate([](const FString& Arg) { return Arg.IsNumeric(); });
		FHollowStructuralHit Hit;
		Hit.Amount = AmountArg ? FCString::Atof(**AmountArg) : TNumericLimits<float>::Max();   // one blow breaks one part at most
		Hit.Location = Pawn->GetActorLocation();
		Hit.Direction = bFromBehind ? Pawn->GetActorLocation() - Nearest->GetActorLocation() : FVector::ZeroVector;   // zero: from the pawn
		Hit.DamageType = HollowTags::Damage_Structural;
		Hit.Instigator = Pawn;
		const bool bChanged = Cast<IHollowStructural>(Nearest)->ApplyStructuralDamage(Hit);
		UE_LOG(LogHollowGame, Log, TEXT("Hollow.Dev.Breach: %s hit%s with %s (%s)"), *Nearest->GetName(), bFromBehind ? TEXT(" from behind") : TEXT(""),
			AmountArg ? **AmountArg : TEXT("a breaking blow"), bChanged ? TEXT("changed") : TEXT("no change"));
	}

	/**
	 * Host: seals the closed door nearest to the local pawn with a full weld at once, as a welder would in a few seconds
	 * (S3-04 checks: the Iron Pilgrim breaking a welded door). Usage: Hollow.Dev.WeldNearestDoor
	 */
	void WeldNearestDoor(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (!Pawn || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.WeldNearestDoor runs on the host with a local pawn."));
			return;
		}
		AHollowDoor* Nearest = nullptr;
		for (TActorIterator<AHollowDoor> It(World); It; ++It)
		{
			if (It->GetDoorState()->Accepts(EHollowDoorEvent::Weld)
				&& (!Nearest || FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) < FVector::DistSquared(Nearest->GetActorLocation(), Pawn->GetActorLocation())))
			{
				Nearest = *It;
			}
		}
		const UHollowDoorDefinition* Definition = Nearest ? Nearest->GetDoorState()->GetDefinition() : nullptr;
		if (!Definition || Definition->WeldPerWorkUnit <= 0.f)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.WeldNearestDoor: no closed door to weld."));
			return;
		}
		Nearest->GetDoorState()->ApplyTool(EHollowDoorEvent::Weld, 1.f / Definition->WeldPerWorkUnit);   // exactly a full weld
		UE_LOG(LogHollowGame, Log, TEXT("Hollow.Dev.WeldNearestDoor: %s %s, weld %.2f"), *Nearest->GetName(),
			*HollowEnumToString(Nearest->GetDoorState()->GetNetState().State), Nearest->GetDoorState()->GetWeld());
	}

	/**
	 * Host: path for one nav agent with its default filter (S2-05), from the local pawn to the extraction marker; lists
	 * the doors on it with their link areas. Usage: Hollow.Nav.Path <Standard|Heavy>
	 */
	void NavPath(const TArray<FString>& Args, UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!Pawn || !Navigation || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Nav.Path runs on the host with a local pawn and a navigation system."));
			return;
		}
		const UHollowNavSettings* Settings = GetDefault<UHollowNavSettings>();
		const bool bHeavy = Args.Num() > 0 && Args[0].Equals(Settings->HeavyAgent.ToString(), ESearchCase::IgnoreCase);
		const FName Agent = bHeavy ? Settings->HeavyAgent : Settings->StandardAgent;
		const TSubclassOf<UNavigationQueryFilter> Filter = (bHeavy ? Settings->HeavyFilter : Settings->StandardFilter).LoadSynchronous();
		ANavigationData* NavData = Navigation->GetNavDataForAgentName(Agent);
		TActorIterator<AHollowExtractionZone> ZoneIt(World);
		const AHollowExtractionZone* Extraction = ZoneIt ? *ZoneIt : nullptr;
		if (!NavData || !Extraction)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Nav.Path: no navmesh for agent %s or no extraction zone."), *Agent.ToString());
			return;
		}
		FPathFindingQuery Query(Pawn, *NavData, Pawn->GetNavAgentLocation(), Extraction->GetActorLocation(), UNavigationQueryFilter::GetQueryFilter(*NavData, Pawn, Filter));
		Query.SetAllowPartialPaths(false);
		const FPathFindingResult Result = Navigation->FindPathSync(Query);
		const bool bFound = Result.IsSuccessful() && Result.Path.IsValid() && !Result.IsPartial();
		FString Doors;
		int32 DoorCount = 0;
		FString Bulkheads;
		int32 BulkheadCount = 0;
		if (bFound)
		{
			for (TActorIterator<AHollowDoor> It(World); It; ++It)
			{
				if (UHollowNavUpdateSubsystem::PathUsesLink(*Result.Path, It->GetNavLink()->GetId()))
				{
					Doors += FString::Printf(TEXT(" %s=%s"), *It->GetName(), *GetNameSafe(It->GetNavArea().Get()));
					++DoorCount;
				}
			}
			for (TActorIterator<AHollowBreachableBulkhead> It(World); It; ++It)
			{
				if (UHollowNavUpdateSubsystem::PathUsesLink(*Result.Path, It->GetNavLink()->GetId()))
				{
					Bulkheads += FString::Printf(TEXT(" %s=%s"), *It->GetName(), *GetNameSafe(It->GetNavArea().Get()));
					++BulkheadCount;
				}
			}
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Nav.Path %s: found %d, length %.0f, %d door(s):%s, %d bulkhead link(s):%s"), *Agent.ToString(), bFound,
			bFound ? Result.Path->GetLength() : 0.0, DoorCount, *Doors, BulkheadCount, *Bulkheads);
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

	FAutoConsoleCommandWithWorldAndArgs RunInRaidCommand(
		TEXT("Hollow.Dev.RunInRaid"),
		TEXT("Runs a console command a delay after this machine is in a ready raid with its pawn (a shared clock for multi-process tests). Usage: Hollow.Dev.RunInRaid <seconds> <command...>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunInRaid));

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

	FAutoConsoleCommandWithWorldAndArgs GoToNearestDoorCommand(
		TEXT("Hollow.Dev.GoToNearestDoor"),
		TEXT("Host: puts the local pawn (or every player's, with All) in front of the nearest door, facing it. Usage: Hollow.Dev.GoToNearestDoor [All]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GoToNearestDoor));

	FAutoConsoleCommandWithWorldAndArgs GoToNearestBulkheadCommand(
		TEXT("Hollow.Dev.GoToNearestBulkhead"),
		TEXT("Host: puts the local pawn (or every player's, with All) in front of the nearest bulkhead, facing it. Usage: Hollow.Dev.GoToNearestBulkhead [All]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GoToNearestBulkhead));

	FAutoConsoleCommandWithWorldAndArgs GoToNearestCoreCommand(
		TEXT("Hollow.Dev.GoToNearestCore"),
		TEXT("Host: puts the local pawn (or every player's, with All) in front of the nearest Aether core, facing it. Usage: Hollow.Dev.GoToNearestCore [All]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GoToNearestCore));

	FAutoConsoleCommandWithWorldAndArgs GoToNearestEnemyCommand(
		TEXT("Hollow.Dev.GoToNearestEnemy"),
		TEXT("Host: puts the local pawn (every player's with All, the remote players' with Others) right in front of the nearest enemy (the director's stalker with Stalker), facing it. Usage: Hollow.Dev.GoToNearestEnemy [All|Others] [Stalker]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GoToNearestEnemy));

	FAutoConsoleCommandWithWorldAndArgs GoToExtractionCommand(
		TEXT("Hollow.Dev.GoToExtraction"),
		TEXT("Host: puts the local pawn (or every player's, with All) into the extraction zone. Usage: Hollow.Dev.GoToExtraction [All]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GoToExtraction));

	FAutoConsoleCommandWithWorldAndArgs KillCommand(
		TEXT("Hollow.Dev.Kill"),
		TEXT("Host: kills the local pawn (every player's with All, the remote players' with Others) with a damage effect of its max health. Usage: Hollow.Dev.Kill [All|Others]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Kill));

	FAutoConsoleCommandWithWorld LightDumpCommand(
		TEXT("Hollow.Light.Dump"),
		TEXT("Prints every streamed room's lights (shadowed, light functions, volumetric shadows) against its type's light budget (any machine)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&LightDump));

	FAutoConsoleCommandWithWorldAndArgs ResonanceAddCommand(
		TEXT("Hollow.Resonance.Add"),
		TEXT("Host: changes the local player's Resonance (every player's with All, the remote players' with Others) by Amount through an effect. Usage: Hollow.Resonance.Add <Amount> [All|Others]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResonanceAdd));

	FAutoConsoleCommandWithWorld ResonanceDumpCommand(
		TEXT("Hollow.Resonance.Dump"),
		TEXT("Prints the squad's replicated Resonance, every player's, and what this machine shows (any machine)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ResonanceDump));

	FAutoConsoleCommandWithWorld RaidDumpCommand(
		TEXT("Hollow.Raid.Dump"),
		TEXT("Prints this machine's raid: phase, time left, players alive and in the zone, cores in the zone; on the host every outcome."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RaidDump));

	FAutoConsoleCommandWithWorld ProfileDumpCommand(
		TEXT("Hollow.Profile.Dump"),
		TEXT("Prints the local profile: credits, loadout and insurance, raid escrow, the debrief not seen yet, counters."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ProfileDump));

	FAutoConsoleCommandWithWorldAndArgs ProfileInsureCommand(
		TEXT("Hollow.Profile.Insure"),
		TEXT("Insures (or, with Off, uninsures) a loadout slot of the local profile. Usage: Hollow.Profile.Insure <Slot> [Off]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ProfileInsure));

	FAutoConsoleCommandWithWorld ProfileResetCommand(
		TEXT("Hollow.Profile.Reset"),
		TEXT("Replaces the local profile with a new one (DefaultGame.ini starting credits and loadout)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ProfileReset));

	FAutoConsoleCommandWithWorldAndArgs BreachCommand(
		TEXT("Hollow.Dev.Breach"),
		TEXT("Host: one structural blow from the local pawn to the nearest breakable object; without Amount it breaks the part nearest to the pawn. Usage: Hollow.Dev.Breach [Amount] [FromBehind]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Breach));

	FAutoConsoleCommandWithWorld WeldNearestDoorCommand(
		TEXT("Hollow.Dev.WeldNearestDoor"),
		TEXT("Host: fully welds the closed door nearest to the local pawn at once. Usage: Hollow.Dev.WeldNearestDoor"),
		FConsoleCommandWithWorldDelegate::CreateStatic(&WeldNearestDoor));

	FAutoConsoleCommandWithWorldAndArgs NavPathCommand(
		TEXT("Hollow.Nav.Path"),
		TEXT("Host: AI path from the local pawn to extraction for a nav agent and its filter. Usage: Hollow.Nav.Path <Standard|Heavy>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NavPath));

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
