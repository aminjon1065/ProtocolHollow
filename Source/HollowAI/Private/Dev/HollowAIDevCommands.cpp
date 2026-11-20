// Development console commands for enemies (headless and multi-process checks). Never shipped.
#include "AbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Brain/HollowAIController.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowAILog.h"
#include "HollowAISettings.h"
#include "Interfaces/HollowStructural.h"
#include "NavigationSystem.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Tags/HollowGameplayTags.h"

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
			UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Dump: enemy %s at %s, yaw %.0f, speed %.0f, health %.0f, winding up %d, dead %d"), *It->GetName(),
				*It->GetActorLocation().ToCompactString(), It->GetActorRotation().Yaw, It->GetVelocity().Size2D(),
				Abilities ? Abilities->GetNumericAttribute(UHollowHealthSet::GetHealthAttribute()) : 0.f,
				Abilities && Abilities->HasMatchingGameplayTag(HollowTags::GameplayCue_Enemy_Windup), It->IsDead());
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

		// The class defaults have not sized their nav agent from the capsule yet: do it here.
		const AHollowEnemyCharacter* Defaults = Class->GetDefaultObject<AHollowEnemyCharacter>();
		FNavAgentProperties Agent = Defaults->GetNavAgentPropertiesRef();
		Agent.AgentRadius = Defaults->GetCapsuleComponent()->GetScaledCapsuleRadius();
		Agent.AgentHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f;
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const ANavigationData* NavData = Navigation ? Navigation->GetNavDataForProps(Agent, Wanted) : nullptr;
		FNavLocation OnNavmesh;
		const FVector Extent(Settings->DevSpawnSearchExtent);
		if (!NavData || !Navigation->ProjectPointToNavigation(Wanted, OnNavmesh, Extent, NavData))
		{
			UE_LOG(LogHollowAI, Warning, TEXT("Hollow.AI.Spawn: no navmesh for %s near %s."), *Class->GetName(), *Wanted.ToCompactString());
			return;
		}
		const FVector Location = OnNavmesh.Location + FVector(0.0, 0.0, Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FRotator Facing = (Player->GetActorLocation() - Location).GetSafeNormal2D().Rotation();
		const AHollowEnemyCharacter* Enemy = World->SpawnActor<AHollowEnemyCharacter>(Class, Location, Facing, Params);
		UE_LOG(LogHollowAI, Display, TEXT("Hollow.AI.Spawn: %s at %s"), *GetNameSafe(Enemy), *Location.ToCompactString());
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

	FAutoConsoleCommandWithWorld KillCommand(
		TEXT("Hollow.AI.Kill"),
		TEXT("Host: kills every enemy (players cannot hurt the Iron Pilgrim yet)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Kill));
}

#endif // !UE_BUILD_SHIPPING
