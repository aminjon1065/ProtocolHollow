// Development console commands for enemies (headless and multi-process checks). Never shipped.
#include "AbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Brain/HollowAIController.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowEnemySpawning.h"
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
	AActor* NearestObstacle(UWorld* World, const FVector& Location)
	{
		AActor* Nearest = nullptr;
		double NearestDistance = TNumericLimits<double>::Max();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const IHollowStructural* Structural = Cast<IHollowStructural>(*It);
			const double Distance = FVector::DistSquared(It->GetActorLocation(), Location);
			if (Structural && Structural->GetPassage() != EHollowPassage::Clear && Distance < NearestDistance)
			{
				Nearest = *It;
				NearestDistance = Distance;
			}
		}
		return Nearest;
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
			UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Dump: enemy %s at %s, yaw %.0f, speed %.0f, health %.0f, winding up %d, stunned %d, in vent %d, hidden %d, dead %d"),
				*It->GetName(), *It->GetActorLocation().ToCompactString(), It->GetActorRotation().Yaw, It->GetVelocity().Size2D(),
				Abilities ? Abilities->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute()) : 0.f,
				Abilities && Abilities->HasMatchingGameplayTag(HollowTags::GameplayCue_Enemy_Windup), Abilities && Abilities->HasMatchingGameplayTag(HollowTags::State_Stunned),
				It->IsInVent(), It->IsHidden(), It->IsDead());
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
			const AActor* Obstacle = NearestObstacle(World, Player->GetActorLocation());
			if (!Obstacle)
			{
				UE_LOG(LogHollowAI, Warning, TEXT("Hollow.AI.Spawn: no closed door or bulkhead to spawn behind."));
				return;
			}
			const FVector Forward = Obstacle->GetActorForwardVector();
			const double Side = FVector::DotProduct(Player->GetActorLocation() - Obstacle->GetActorLocation(), Forward) >= 0.0 ? 1.0 : -1.0;
			Wanted = Obstacle->GetActorLocation() - Forward * Side * Distance;
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

	FAutoConsoleCommandWithWorld KillCommand(
		TEXT("Hollow.AI.Kill"),
		TEXT("Host: kills every enemy (players cannot hurt the Iron Pilgrim yet)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Kill));
}

#endif // !UE_BUILD_SHIPPING
