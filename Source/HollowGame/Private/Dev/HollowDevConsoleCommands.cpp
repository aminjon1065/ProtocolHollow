// Development console commands for headless and multi-process testing. Never shipped.
#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowToolSet.h"
#include "Breach/HollowBreachableBulkhead.h"
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

	/**
	 * Host: puts Player's pawn in front of its nearest actor of Class (a door or a bulkhead, both face X across their
	 * opening), facing it, Offset uu sideways along it.
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
		// Stay on the pawn's side of the door, well inside interaction and tool reach.
		const FVector Across = Nearest->GetActorForwardVector();
		const double Side = FVector::DotProduct(Character->GetActorLocation() - Nearest->GetActorLocation(), Across) >= 0.0 ? 1.0 : -1.0;
		const float Distance = GetDefault<UHollowInteractionSettings>()->InteractionRange * 0.5f;
		const FVector Stand = Nearest->GetActorLocation() + Across * Side * Distance + Nearest->GetActorRightVector() * Offset
			+ FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Character->SetActorLocation(Stand, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		FVector Centre;
		FVector Extent;
		Nearest->GetActorBounds(/*bOnlyCollidingComponents=*/true, Centre, Extent);
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
	 * Host: puts the local pawn (or, with All, every player's pawn, side by side) in front of the nearest actor of Class,
	 * facing it (S2-04/S2-06/S3-01 tests).
	 */
	void GoToNearest(const TArray<FString>& Args, UWorld* World, const TSubclassOf<AActor> Class, const TCHAR* Command)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("%s runs on the host."), Command);
			return;
		}
		const bool bAll = Args.Num() > 0 && Args[0].Equals(TEXT("All"), ESearchCase::IgnoreCase);
		TArray<APlayerController*> Players;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* Player = It->Get(); Player && (bAll || Player->IsLocalController()))
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
		const AHollowSectorMarker* Extraction = nullptr;
		for (TActorIterator<AHollowSectorMarker> It(World); It && !Extraction; ++It)
		{
			Extraction = It->GetLabel() == HollowTags::Socket_Extraction.GetTag().GetTagName() ? *It : nullptr;
		}
		if (!NavData || !Extraction)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Nav.Path: no navmesh for agent %s or no extraction marker."), *Agent.ToString());
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

	FAutoConsoleCommandWithWorldAndArgs BreachCommand(
		TEXT("Hollow.Dev.Breach"),
		TEXT("Host: one structural blow from the local pawn to the nearest breakable object; without Amount it breaks the part nearest to the pawn. Usage: Hollow.Dev.Breach [Amount] [FromBehind]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Breach));

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
