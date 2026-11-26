// Development console commands for enemies (headless and multi-process checks). Never shipped.
#include "AbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Brain/HollowAIController.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowEnemySpawning.h"
#include "Enemies/HollowWhisperer.h"
#include "Enemies/HollowWhispererNest.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowAILog.h"
#include "HollowAISettings.h"
#include "Interfaces/HollowStructural.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Tags/HollowGameplayTags.h"
#include "Threat/HollowActivityHeatmapSubsystem.h"
#include "Threat/HollowSwarmSubsystem.h"
#include "Threat/HollowThreatDirectorSubsystem.h"
#include "Threat/HollowVentNetworkSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace HollowAIDevConsole
{
	APawn* LocalPawn(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		return Player ? Player->GetPawn() : nullptr;
	}

	bool IsServer(const UWorld* World, const TCHAR* Command)
	{
		const bool bServer = World && World->GetNetMode() != NM_Client;
		UE_CLOG(!bServer, LogHollowAI, Warning, TEXT("%s: host only (enemies run on the server)."), Command);
		return bServer;
	}

	/** Nearest door or bulkhead that does not let an enemy through, to Location. */
	/** Closed doors and bulkheads, nearest to Location first. */
	TArray<AActor*> ObstaclesByDistance(UWorld* World, const FVector& Location)
	{
		TArray<AActor*> Obstacles;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const IHollowStructural* Structural = Cast<IHollowStructural>(*It);
			if (Structural && Structural->GetPassage() != EHollowPassage::Clear)
			{
				Obstacles.Add(*It);
			}
		}
		Obstacles.Sort([&Location](const AActor& A, const AActor& B)
		{
			return FVector::DistSquared(A.GetActorLocation(), Location) < FVector::DistSquared(B.GetActorLocation(), Location);
		});
		return Obstacles;
	}

	void Dump(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		// What every machine sees of the enemies (replicated), then the brains (server only).
		for (TActorIterator<AHollowEnemyCharacter> It(World); It; ++It)
		{
			const UAbilitySystemComponent* Abilities = It->GetAbilitySystemComponent();
			// GP-02: a Whisperer's replicated light state and what this machine played for it.
			const AHollowWhisperer* Whisperer = Cast<AHollowWhisperer>(*It);
			const FString Swarm = Whisperer ? FString::Printf(TEXT(", lit %d, frenzy %d, shrieks %d, whispering %d"),
				Abilities && Abilities->HasMatchingGameplayTag(HollowTags::State_Lit), Abilities && Abilities->HasMatchingGameplayTag(HollowTags::State_Frenzy),
				Whisperer->GetShriekCount(), Whisperer->GetWhisper() != nullptr) : FString();
			UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Dump: enemy %s at %s, yaw %.0f, speed %.0f, health %.0f, winding up %d, stunned %d, in vent %d, hidden %d, dead %d%s"),
				*It->GetName(), *It->GetActorLocation().ToCompactString(), It->GetActorRotation().Yaw, It->GetVelocity().Size2D(),
				Abilities ? Abilities->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute()) : 0.f,
				Abilities && Abilities->HasMatchingGameplayTag(HollowTags::GameplayCue_Enemy_Windup), Abilities && Abilities->HasMatchingGameplayTag(HollowTags::State_Stunned),
				It->IsInVent(), It->IsHidden(), It->IsDead(), *Swarm);
		}
		for (TActorIterator<AHollowAIController> It(World); It; ++It)
		{
			UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Dump: brain %s"), *It->Describe());
		}
	}

	/** Usage: Hollow.AI.Spawn [Behind] [Distance] */
	void Spawn(const TArray<FString>& Args, UWorld* World)
	{
		const UHollowAISettings* Settings = GetDefault<UHollowAISettings>();
		APawn* Player = LocalPawn(World);
		const TSubclassOf<AHollowEnemyCharacter> Class = Settings->DevEnemyClass.LoadSynchronous();
		if (!IsServer(World, TEXT("Hollow.AI.Spawn")) || !Player || !Class)
		{
			UE_LOG(LogHollowAI, Warning, TEXT("Hollow.AI.Spawn: needs a local pawn and UHollowAISettings::DevEnemyClass."));
			return;
		}
		const bool bBehind = Args.Num() > 0 && Args[0].Equals(TEXT("Behind"), ESearchCase::IgnoreCase);
		const int32 DistanceArg = bBehind ? 1 : 0;
		const double Distance = Args.IsValidIndex(DistanceArg) ? FCString::Atod(*Args[DistanceArg]) : Settings->DevSpawnDistance;

		FVector Wanted = Player->GetActorLocation() + Player->GetActorForwardVector() * Distance;
		if (bBehind)
		{
			// The nearest closed door or bulkhead with navmesh behind it (a door to the hull has none).
			bool bFound = false;
			for (const AActor* Obstacle : ObstaclesByDistance(World, Player->GetActorLocation()))
			{
				const FVector Forward = Obstacle->GetActorForwardVector();
				const double Side = FVector::DotProduct(Player->GetActorLocation() - Obstacle->GetActorLocation(), Forward) >= 0.0 ? 1.0 : -1.0;
				Wanted = Obstacle->GetActorLocation() - Forward * Side * Distance;
				if (HollowEnemySpawning::HasNavmeshFor(World, Class, Wanted, Settings->DevSpawnSearchExtent))
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				UE_LOG(LogHollowAI, Warning, TEXT("Hollow.AI.Spawn: no closed door or bulkhead with room behind it."));
				return;
			}
		}

		const FRotator Facing = (Player->GetActorLocation() - Wanted).GetSafeNormal2D().Rotation();
		const AHollowEnemyCharacter* Enemy = HollowEnemySpawning::SpawnOnNavmesh(World, Class, Wanted, Settings->DevSpawnSearchExtent, Facing);
		UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Spawn: %s at %s"), *GetNameSafe(Enemy), Enemy ? *Enemy->GetActorLocation().ToCompactString() : TEXT("-"));
	}

	/** Usage: Hollow.AI.Noise [Hollow.Noise.Tag] */
	void Noise(const TArray<FString>& Args, UWorld* World)
	{
		APawn* Player = LocalPawn(World);
		UHollowNoiseSubsystem* Noises = UHollowNoiseSubsystem::Get(World);
		if (!IsServer(World, TEXT("Hollow.AI.Noise")) || !Player || !Noises)
		{
			return;
		}
		const FGameplayTag Tag = Args.Num() > 0 ? FGameplayTag::RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound=*/false) : GetDefault<UHollowAISettings>()->DevNoiseTag;
		const bool bReported = Noises->ReportNoise(FHollowNoiseEvent(Tag, Player->GetActorLocation(), Player));
		UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Noise: %s at %s%s"), *Tag.ToString(), *Player->GetActorLocation().ToCompactString(), bReported ? TEXT("") : TEXT(" (not reported)"));
	}

	/** Usage: Hollow.AI.Nest [Distance] [Seed] */
	void Nest(const TArray<FString>& Args, UWorld* World)
	{
		const UHollowAISettings* Settings = GetDefault<UHollowAISettings>();
		APawn* Player = LocalPawn(World);
		const TSubclassOf<AHollowWhispererNest> Class = Settings->DevNestClass.LoadSynchronous();
		if (!IsServer(World, TEXT("Hollow.AI.Nest")) || !Player || !Class)
		{
			UE_LOG(LogHollowAI, Warning, TEXT("Hollow.AI.Nest: needs a local pawn and UHollowAISettings::DevNestClass."));
			return;
		}
		const double Distance = Args.Num() > 0 ? FCString::Atod(*Args[0]) : Settings->DevSpawnDistance;
		const int32 Seed = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
		// On the floor in front of the player (the nest's pivot is its base).
		FVector Where = Player->GetActorLocation() + Player->GetActorForwardVector() * Distance;
		FHitResult Floor;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowDevNest), /*bTraceComplex=*/false, Player);
		if (World->LineTraceSingleByChannel(Floor, Where, Where - FVector(0.0, 0.0, 1000.0), ECC_Visibility, Params))
		{
			Where = Floor.ImpactPoint;
		}
		const FTransform Transform(FRotator::ZeroRotator, Where);
		AHollowWhispererNest* Spawned = World->SpawnActorDeferred<AHollowWhispererNest>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Spawned)
		{
			return;
		}
		Spawned->SetSpawnSeed(Seed);
		Spawned->FinishSpawning(Transform);
		UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Nest: %s (seed %d, brood %d) at %s"), *Spawned->GetName(), Seed, Spawned->GetBroodSize(), *Where.ToCompactString());
	}

	/** Usage: Hollow.AI.Swarm [Check] */
	void Swarm(const TArray<FString>& Args, UWorld* World)
	{
		UHollowSwarmSubsystem* Swarms = UHollowSwarmSubsystem::Get(World);
		if (!IsServer(World, TEXT("Hollow.AI.Swarm")) || !Swarms)
		{
			return;
		}
		if (Args.Num() > 0 && Args[0].Equals(TEXT("Check"), ESearchCase::IgnoreCase))
		{
			Swarms->CheckLight();
		}
		UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Swarm: %s"), *Swarms->Describe());
	}

	void Kill(UWorld* World)
	{
		if (!IsServer(World, TEXT("Hollow.AI.Kill")))
		{
			return;
		}
		for (TActorIterator<AHollowEnemyCharacter> It(World); It; ++It)
		{
			It->Die();
		}
	}

	void Heat(UWorld* World)
	{
		const UHollowActivityHeatmapSubsystem* Heatmap = UHollowActivityHeatmapSubsystem::Get(World);
		if (IsServer(World, TEXT("Hollow.AI.Heat")) && Heatmap)
		{
			UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Heat: %s"), *Heatmap->Describe());
		}
	}

	void Vents(UWorld* World)
	{
		const UHollowVentNetworkSubsystem* Network = UHollowVentNetworkSubsystem::Get(World);
		if (IsServer(World, TEXT("Hollow.AI.Vents")) && Network)
		{
			UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Vents: %s"), *Network->Describe());
		}
	}

	/** Usage: Hollow.AI.Director [Stalk|Hunt|Retreat|Spawn] */
	void Director(const TArray<FString>& Args, UWorld* World)
	{
		UHollowThreatDirectorSubsystem* Threat = UHollowThreatDirectorSubsystem::Get(World);
		if (!IsServer(World, TEXT("Hollow.AI.Director")) || !Threat)
		{
			return;
		}
		const FString Order = Args.Num() > 0 ? Args[0] : FString();
		if (Order.Equals(TEXT("Spawn"), ESearchCase::IgnoreCase))
		{
			UE_CLOG(Threat->GetStalker(), LogHollowAI, Warning, TEXT("Hollow.AI.Director: the stalker is already here."));
			if (!Threat->GetStalker())
			{
				Threat->SpawnStalker();
			}
		}
		else if (!Order.IsEmpty())
		{
			const int64 Mode = StaticEnum<EHollowThreatMode>()->GetValueByNameString(Order);
			UE_CLOG(Mode == INDEX_NONE, LogHollowAI, Warning, TEXT("Hollow.AI.Director: unknown order %s (Stalk, Hunt, Retreat, Spawn)."), *Order);
			if (Mode != INDEX_NONE)
			{
				Threat->SetMode(static_cast<EHollowThreatMode>(Mode));
			}
		}
		UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Director: %s"), *Threat->Describe());
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.AI.Dump"),
		TEXT("Prints the enemies as this machine sees them (any machine) and their brains (host): activity, states, target, stimulus, obstacle."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));

	FAutoConsoleCommandWithWorldAndArgs SpawnCommand(
		TEXT("Hollow.AI.Spawn"),
		TEXT("Host: spawns UHollowAISettings::DevEnemyClass Distance in front of the local pawn, or Behind the nearest closed door or bulkhead. Usage: Hollow.AI.Spawn [Behind] [Distance]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Spawn));

	FAutoConsoleCommandWithWorldAndArgs NoiseCommand(
		TEXT("Hollow.AI.Noise"),
		TEXT("Host: makes a noise at the local pawn through UHollowNoiseSubsystem (UHollowAISettings::DevNoiseTag without a tag). Usage: Hollow.AI.Noise [Hollow.Noise.Tag]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Noise));

	FAutoConsoleCommandWithWorld HeatCommand(
		TEXT("Hollow.AI.Heat"),
		TEXT("Host: prints the activity heat of the rooms, the squad's room and the hottest room nobody is in."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Heat));

	FAutoConsoleCommandWithWorld VentsCommand(
		TEXT("Hollow.AI.Vents"),
		TEXT("Host: prints the vent openings and links of the sector."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Vents));

	FAutoConsoleCommandWithWorldAndArgs DirectorCommand(
		TEXT("Hollow.AI.Director"),
		TEXT("Host: prints the director's leash; Stalk, Hunt or Retreat forces a mode, Spawn brings the stalker now. Usage: Hollow.AI.Director [Stalk|Hunt|Retreat|Spawn]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Director));

	FAutoConsoleCommandWithWorldAndArgs NestCommand(
		TEXT("Hollow.AI.Nest"),
		TEXT("Host: puts UHollowAISettings::DevNestClass on the floor Distance in front of the local pawn; its brood comes out the next tick. Usage: Hollow.AI.Nest [Distance] [Seed]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Nest));

	FAutoConsoleCommandWithWorldAndArgs SwarmCommand(
		TEXT("Hollow.AI.Swarm"),
		TEXT("Host: prints the Whisperer swarms (lit, in a frenzy, biting); Check runs the light check now. Usage: Hollow.AI.Swarm [Check]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Swarm));

	FAutoConsoleCommandWithWorld KillCommand(
		TEXT("Hollow.AI.Kill"),
		TEXT("Host: kills every enemy (players cannot hurt the Iron Pilgrim yet)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Kill));
}

#endif // !UE_BUILD_SHIPPING
