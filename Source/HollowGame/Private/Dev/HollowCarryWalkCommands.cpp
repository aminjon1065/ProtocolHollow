// Development console commands that walk players like a player would (real input, so client prediction runs), for the
// co-op carry smoke (S3-06, ADR-0028): open the doors, walk a route of rooms, follow the carry partner, face the core.
// Never shipped.
#include "Assembly/HollowSectorAssemblySubsystem.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Containers/Ticker.h"
#include "Data/HollowLevelMetrics.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Sector/HollowSectorTopologyBuilder.h"
#include "Character/HollowExoFrameCharacter.h"

#if !UE_BUILD_SHIPPING

namespace HollowCarryWalkDev
{
	// Steering of the dev walkers (not gameplay tuning).
	constexpr double ReachedDistance = 40.0;        // cm: a waypoint counts as reached this close (along the ground)
	constexpr double PortalLead = 100.0;            // cm before and after an opening: cross it square, clear of its frame
	constexpr double ArrivalDepth = 250.0;          // cm into the last room: the walk ends there (room centres may hold props)
	constexpr double StuckSeconds = 4.0;            // no progress for this long: give up
	constexpr double ProgressDistance = 20.0;       // cm closer to the waypoint counts as progress
	constexpr double BreadcrumbSpacing = 25.0;      // cm between the leader positions a follower walks through
	constexpr double FollowerGiveUpSeconds = 1.0;   // without a partner this long, the follower stops
	constexpr double TeleportDistance = 200.0;      // cm in one frame: the pawn was put somewhere else, the walk is over

	struct FWalker
	{
		TWeakObjectPtr<AHollowExoFrameCharacter> Pawn;
		TArray<FVector> Waypoints;
		int32 Next = 0;
		double Best = TNumericLimits<double>::Max();
		double SinceProgress = 0.0;
		double Elapsed = 0.0;
		int32 Rooms = 0;
		int32 CorrectionsAtStart = 0;
		FVector LastLocation = FVector::ZeroVector;
	};

	struct FFollower
	{
		TWeakObjectPtr<AHollowExoFrameCharacter> Pawn;
		TArray<FVector> Breadcrumbs;
		double WithoutPartner = 0.0;
		int32 CorrectionsAtStart = 0;
	};

	FTSTicker::FDelegateHandle WalkHandle;
	FTSTicker::FDelegateHandle FollowHandle;

	AHollowExoFrameCharacter* LocalPawn(UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		return Controller ? Cast<AHollowExoFrameCharacter>(Controller->GetPawn()) : nullptr;
	}

	int32 Corrections(const AHollowExoFrameCharacter& Pawn)
	{
		return Pawn.GetHollowMovement() ? Pawn.GetHollowMovement()->GetCorrectionCount() : 0;
	}

	/** Input towards Target along the ground, facing it, as a player holding the stick would. */
	void Steer(AHollowExoFrameCharacter& Pawn, const FVector& Target)
	{
		FVector Direction = Target - Pawn.GetActorLocation();
		Direction.Z = 0.0;
		if (!Direction.Normalize())
		{
			return;
		}
		Pawn.AddMovementInput(Direction, 1.f);
		if (AController* Controller = Pawn.GetController())
		{
			Controller->SetControlRotation(FRotator(0.0, Direction.Rotation().Yaw, 0.0));
		}
	}

	/** The assembled sector as rooms and openings, built here from the manifest (clients have no AI topology). */
	FHollowSectorTopology BuildTopology(UWorld& World)
	{
		const UHollowSectorAssemblySubsystem* Assembly = World.GetSubsystem<UHollowSectorAssemblySubsystem>();
		if (!Assembly || !Assembly->IsAssembled())
		{
			return FHollowSectorTopology();
		}
		return HollowSectorTopologyBuilder::Build(Assembly->GetManifest(), Assembly->GetLayout(), *GetDefault<UHollowLevelMetrics>(),
			UHollowSectorAssemblySubsystem::GetSectorOrigin());
	}

	/**
	 * Waypoints from Start through Rooms openings (bulkheads excluded: they stay shut until breached) to the centre of
	 * the room reached, the first one that far in manifest order (or the furthest one there is).
	 */
	TArray<FVector> PlanRoute(const FHollowSectorTopology& Topology, const int32 Start, const int32 Rooms, int32& OutRooms)
	{
		TArray<int32> Parent;
		TArray<int32> Distance;
		Parent.Init(INDEX_NONE, Topology.Rooms.Num());
		Distance.Init(INDEX_NONE, Topology.Rooms.Num());
		TArray<int32> Queue = {Start};
		Distance[Start] = 0;
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 Room = Queue[Head];
			for (const FHollowTopologyPortal& Portal : Topology.Portals)
			{
				const int32 Other = Portal.RoomA == Room ? Portal.RoomB : Portal.RoomB == Room ? Portal.RoomA : INDEX_NONE;
				if (Other != INDEX_NONE && Portal.Kind != EHollowPortalKind::Bulkhead && Distance[Other] == INDEX_NONE)
				{
					Distance[Other] = Distance[Room] + 1;
					Parent[Other] = Room;
					Queue.Add(Other);
				}
			}
		}
		int32 Target = Start;
		for (int32 Room = 0; Room < Distance.Num(); ++Room)
		{
			if (Distance[Room] != INDEX_NONE && Distance[Room] <= Rooms && Distance[Room] > Distance[Target])
			{
				Target = Room;
			}
		}
		TArray<int32> Path;
		for (int32 Room = Target; Room != INDEX_NONE; Room = Parent[Room])
		{
			Path.Insert(Room, 0);
		}
		TArray<FVector> Waypoints;
		FVector Across = FVector::ZeroVector;
		for (int32 Step = 0; Step + 1 < Path.Num(); ++Step)
		{
			const int32 PortalIndex = Topology.PortalBetween(Path[Step], Path[Step + 1]);
			const FVector Opening = Topology.Portals[PortalIndex].Location;
			// Square to the wall the opening is in: from the room's centre towards it, along its main axis.
			Across = Opening - Topology.Rooms[Path[Step]].Bounds.GetCenter();
			Across = FMath::Abs(Across.X) >= FMath::Abs(Across.Y) ? FVector(FMath::Sign(Across.X), 0.0, 0.0) : FVector(0.0, FMath::Sign(Across.Y), 0.0);
			Waypoints.Add(Opening - Across * PortalLead);
			Waypoints.Add(Opening + Across * PortalLead);
		}
		// Straight on into the last room (its centre may hold a prop); without a route, its centre.
		Waypoints.Add(Waypoints.IsEmpty() ? Topology.Rooms[Target].Bounds.GetCenter() : Waypoints.Last() + Across * (ArrivalDepth - PortalLead));
		OutRooms = Path.Num() - 1;
		return Waypoints;
	}

	bool TickWalk(const float DeltaSeconds, const TSharedRef<FWalker> Walker)
	{
		AHollowExoFrameCharacter* Pawn = Walker->Pawn.Get();
		if (!Pawn || !Pawn->GetController())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.WalkRoute: the pawn is gone, stopped."));
			return false;
		}
		Walker->Elapsed += DeltaSeconds;
		if (FVector::Dist2D(Pawn->GetActorLocation(), Walker->LastLocation) > TeleportDistance)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.WalkRoute: %s was moved away, walk over"), *Pawn->GetName());
			return false;
		}
		Walker->LastLocation = Pawn->GetActorLocation();
		const FVector Target = Walker->Waypoints[Walker->Next];
		const double Distance = FVector::Dist2D(Pawn->GetActorLocation(), Target);
		if (Distance < ReachedDistance)
		{
			if (++Walker->Next == Walker->Waypoints.Num())
			{
				UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.WalkRoute: arrived after %d room(s) in %.1f s at %s, %d correction(s) on the way"),
					Walker->Rooms, Walker->Elapsed, *Pawn->GetActorLocation().ToCompactString(), Corrections(*Pawn) - Walker->CorrectionsAtStart);
				return false;
			}
			Walker->Best = TNumericLimits<double>::Max();
			Walker->SinceProgress = 0.0;
			return true;
		}
		if (Distance < Walker->Best - ProgressDistance)
		{
			Walker->Best = Distance;
			Walker->SinceProgress = 0.0;
		}
		else if ((Walker->SinceProgress += DeltaSeconds) > StuckSeconds)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.WalkRoute: stuck at %s, %.0f cm from waypoint %d of %d"), *Pawn->GetActorLocation().ToCompactString(),
				Distance, Walker->Next + 1, Walker->Waypoints.Num());
			return false;
		}
		Steer(*Pawn, Target);
		return true;
	}

	/** Usage: Hollow.Dev.WalkRoute <rooms>: the local player walks that many rooms on (through open doors). */
	void WalkRoute(const TArray<FString>& Args, UWorld* World)
	{
		AHollowExoFrameCharacter* Pawn = LocalPawn(World);
		const int32 Rooms = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		const FHollowSectorTopology Topology = World ? BuildTopology(*World) : FHollowSectorTopology();
		const int32 Start = Pawn ? Topology.RoomAt(Pawn->GetActorLocation()) : INDEX_NONE;
		if (!Pawn || Start == INDEX_NONE || Rooms < 1)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.WalkRoute <rooms>: needs a local pawn in an assembled sector."));
			return;
		}
		TSharedRef<FWalker> Walker = MakeShared<FWalker>();
		Walker->Pawn = Pawn;
		Walker->Waypoints = PlanRoute(Topology, Start, Rooms, Walker->Rooms);
		Walker->CorrectionsAtStart = Corrections(*Pawn);
		Walker->LastLocation = Pawn->GetActorLocation();
		FTSTicker::GetCoreTicker().RemoveTicker(WalkHandle);
		WalkHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Walker](const float DeltaSeconds)
		{
			return TickWalk(DeltaSeconds, Walker);
		}));
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.WalkRoute: %s walks %d room(s) from room %d, %d waypoint(s)"), *Pawn->GetName(), Walker->Rooms, Start,
			Walker->Waypoints.Num());
	}

	bool TickFollow(const float DeltaSeconds, const TSharedRef<FFollower> Follower)
	{
		AHollowExoFrameCharacter* Pawn = Follower->Pawn.Get();
		const UHollowCarrierComponent* Carrier = Pawn ? Pawn->GetCarrierComponent() : nullptr;
		const APawn* Partner = Carrier ? Carrier->GetPartner() : nullptr;
		if (!Partner)
		{
			if (!Pawn || (Follower->WithoutPartner += DeltaSeconds) > FollowerGiveUpSeconds)
			{
				UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.FollowCarry: no partner, stopped (%d correction(s) while following)"),
					Pawn ? Corrections(*Pawn) - Follower->CorrectionsAtStart : 0);
				return false;
			}
			return true;
		}
		Follower->WithoutPartner = 0.0;
		const FVector Lead = Partner->GetActorLocation();
		if (Follower->Breadcrumbs.IsEmpty() || FVector::Dist2D(Follower->Breadcrumbs.Last(), Lead) > BreadcrumbSpacing)
		{
			Follower->Breadcrumbs.Add(Lead);
		}
		// Walk only towards the leader along its path: a breadcrumb reached, or no nearer the leader than this pawn is
		// (left behind, e.g. where the leader stood when following began), is dropped.
		const double ToLead = FVector::Dist2D(Pawn->GetActorLocation(), Lead);
		while (Follower->Breadcrumbs.Num() > 1 && (FVector::Dist2D(Follower->Breadcrumbs[0], Pawn->GetActorLocation()) < ReachedDistance
			|| FVector::Dist2D(Follower->Breadcrumbs[0], Lead) >= ToLead))
		{
			Follower->Breadcrumbs.RemoveAt(0);
		}
		// Keep the carry spacing like a player would: walk the leader's path while further away than that, back off
		// while the leader comes closer than the tether lets it (it cannot walk through the core), stand otherwise.
		const UHollowCarryableComponent* Carried = Carrier->GetCarried();
		const FHollowCoopCarryTuning* Tether = Carried && Carried->GetDefinition() ? &Carried->GetDefinition()->Coop : nullptr;
		if (Tether && ToLead > Tether->CarrierSpacing)
		{
			Steer(*Pawn, Follower->Breadcrumbs[0]);
		}
		else if (Tether && ToLead < Tether->CarrierSpacing - Tether->Slack)
		{
			Steer(*Pawn, Pawn->GetActorLocation() * 2.0 - Lead);   // straight away from the leader
		}
		return true;
	}

	/** Usage: Hollow.Dev.FollowCarry: the local player, carrying with someone, walks after them until the carry ends. */
	void FollowCarry(UWorld* World)
	{
		AHollowExoFrameCharacter* Pawn = LocalPawn(World);
		if (!Pawn)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.FollowCarry: no local pawn."));
			return;
		}
		TSharedRef<FFollower> Follower = MakeShared<FFollower>();
		Follower->Pawn = Pawn;
		Follower->CorrectionsAtStart = Corrections(*Pawn);
		FTSTicker::GetCoreTicker().RemoveTicker(FollowHandle);
		FollowHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Follower](const float DeltaSeconds)
		{
			return TickFollow(DeltaSeconds, Follower);
		}));
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.FollowCarry: %s follows its carry partner"), *Pawn->GetName());
	}

	/** Usage: Hollow.Dev.FaceCore: the local player looks at the nearest carryable (to focus it for Interact). */
	void FaceCore(UWorld* World)
	{
		AHollowExoFrameCharacter* Pawn = LocalPawn(World);
		const AActor* Nearest = nullptr;
		for (TActorIterator<AActor> It(World); It && Pawn; ++It)
		{
			if (It->FindComponentByClass<UHollowCarryableComponent>()
				&& (!Nearest || FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) < FVector::DistSquared(Nearest->GetActorLocation(), Pawn->GetActorLocation())))
			{
				Nearest = *It;
			}
		}
		AController* Controller = Pawn ? Pawn->GetController() : nullptr;
		if (!Nearest || !Controller)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.FaceCore: no local pawn or no core."));
			return;
		}
		FVector Eyes;
		FRotator Unused;
		Pawn->GetActorEyesViewPoint(Eyes, Unused);
		Controller->SetControlRotation((Nearest->GetActorLocation() - Eyes).Rotation());
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.FaceCore: %s faces %s, %.0f cm away"), *Pawn->GetName(), *Nearest->GetName(),
			FVector::Dist(Eyes, Nearest->GetActorLocation()));
	}

	/** Usage: Hollow.Dev.OpenDoors (host): opens every closed door of the sector, so a route can be walked. */
	void OpenDoors(UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Hollow.Dev.OpenDoors runs on the host."));
			return;
		}
		int32 Opened = 0;
		for (TActorIterator<AHollowDoor> It(World); It; ++It)
		{
			UHollowDoorStateComponent* Door = It->GetDoorState();
			if (Door && Door->GetNetState().State == EHollowDoorState::Closed && Door->Interact(/*bDoorwayClear=*/true) == EHollowDoorOutcome::StateChanged)
			{
				++Opened;
			}
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Dev.OpenDoors: %d door(s) opening"), Opened);
	}

	FAutoConsoleCommandWithWorldAndArgs WalkRouteCommand(
		TEXT("Hollow.Dev.WalkRoute"),
		TEXT("Usage: Hollow.Dev.WalkRoute <rooms>. The local player walks with real input through that many rooms (open doors first: Hollow.Dev.OpenDoors)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WalkRoute));

	FAutoConsoleCommandWithWorld FollowCarryCommand(
		TEXT("Hollow.Dev.FollowCarry"),
		TEXT("The local player, carrying with someone, walks after its partner (keeping the carry spacing) until the carry ends."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&FollowCarry));

	FAutoConsoleCommandWithWorld FaceCoreCommand(
		TEXT("Hollow.Dev.FaceCore"),
		TEXT("The local player looks at the nearest carryable (focus it, then Hollow.Interact)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&FaceCore));

	FAutoConsoleCommandWithWorld OpenDoorsCommand(
		TEXT("Hollow.Dev.OpenDoors"),
		TEXT("Host: opens every closed door of the sector."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&OpenDoors));
}

#endif // !UE_BUILD_SHIPPING
