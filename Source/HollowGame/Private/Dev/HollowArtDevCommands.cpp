#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/HollowCosmeticParts.h"
#include "Data/HollowToolDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Tools/HollowLanternCue.h"
#include "Tools/HollowToolMountComponent.h"

/** Art-01 (ADR-0043): pictures of the art kit in a running game (the smoke's "art" mode, the owner's art pass). */
namespace HollowArtDev
{
	constexpr double DefaultForward = 300.0;
	constexpr int32 ForwardArg = 1;
	constexpr int32 RightArg = 2;
	constexpr int32 YawArg = 1;

	APlayerController* LocalPlayer(const UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	/**
	 * Usage: Hollow.Art.Showcase <ClassPath> [Forward] [Right]: spawns the class Forward cm in front of the local pawn and
	 * Right cm to its right, facing it; a pawn comes without its AI, so it only stands there to be looked at.
	 */
	void Showcase(const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* Player = LocalPlayer(World);
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		UClass* Class = Args.IsEmpty() ? nullptr : LoadClass<AActor>(nullptr, *Args[0]);
		if (!Pawn || !Class)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (with a local pawn): Hollow.Art.Showcase <ClassPath> [Forward] [Right]"));
			return;
		}
		const double Forward = Args.IsValidIndex(ForwardArg) ? FCString::Atod(*Args[ForwardArg]) : DefaultForward;
		const double Right = Args.IsValidIndex(RightArg) ? FCString::Atod(*Args[RightArg]) : 0.0;
		const FRotator View(0.0, Player->GetControlRotation().Yaw, 0.0);
		const FVector Location = Pawn->GetActorLocation() + View.RotateVector(FVector(Forward, Right, 0.0));
		const FRotator Facing = (Pawn->GetActorLocation() - Location).GetSafeNormal2D().Rotation();
		AActor* Actor = World->SpawnActorDeferred<AActor>(Class, FTransform(Facing, Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (APawn* Shown = Cast<APawn>(Actor))
		{
			Shown->AutoPossessAI = EAutoPossessAI::Disabled;
		}
		if (Actor)
		{
			Actor->FinishSpawning(FTransform(Facing, Location));
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Art.Showcase: %s at %s"), *GetNameSafe(Actor), *Location.ToCompactString());
	}

	/** Usage: Hollow.Art.View <Pitch> [Yaw]: the local player looks there (Yaw kept when not given). */
	void View(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* Player = LocalPlayer(World);
		if (!Player || Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage (with a local player): Hollow.Art.View <Pitch> [Yaw]"));
			return;
		}
		FRotator Rotation = Player->GetControlRotation();
		Rotation.Pitch = FCString::Atof(*Args[0]);
		if (Args.IsValidIndex(YawArg))
		{
			Rotation.Yaw = FCString::Atof(*Args[YawArg]);
		}
		Player->SetControlRotation(Rotation);
	}

	/** Prints every character's cosmetic parts and every held tool on this machine. */
	void Dump(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (const UHollowCosmeticPartsComponent* Parts = UHollowCosmeticPartsComponent::Get(*It))
			{
				UE_LOG(LogHollowGame, Display, TEXT("Hollow.Art.Dump: %s parts %d%s tint #%s"), *It->GetName(), Parts->GetPartComponents().Num(),
					Parts->IsLoading() ? TEXT(" (loading)") : TEXT(""), *Parts->GetTint().ToFColor(true).ToHex());
			}
			if (const AHollowLanternCue* Lantern = Cast<AHollowLanternCue>(*It))
			{
				const USpotLightComponent* Light = Lantern->GetLight();
				UE_LOG(LogHollowGame, Display, TEXT("Hollow.Art.Dump: lantern light at %s facing %s, intensity %.0f (%s)"), *Light->GetComponentLocation().ToCompactString(),
					*Light->GetComponentRotation().ToCompactString(), Light->Intensity, Light->IsVisible() ? TEXT("on") : TEXT("off"));
			}
			if (const UHollowToolMountComponent* Mount = UHollowToolMountComponent::Get(*It))
			{
				const UStaticMeshComponent* Held = Mount->GetFirstPersonMesh();
				UE_LOG(LogHollowGame, Display, TEXT("Hollow.Art.Dump: %s holds %s (%s), firing %d"), *It->GetName(), *GetNameSafe(Mount->GetShownTool()),
					Held ? *GetNameSafe(Held->GetStaticMesh()) : TEXT("no mesh"), Mount->IsFiring());
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs ShowcaseCommand(
		TEXT("Hollow.Art.Showcase"),
		TEXT("Spawns a class in front of the local pawn, facing it, a pawn without its AI. Usage: Hollow.Art.Showcase <ClassPath> [Forward] [Right]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Showcase));

	FAutoConsoleCommandWithWorldAndArgs ViewCommand(
		TEXT("Hollow.Art.View"),
		TEXT("The local player looks at a pitch (and yaw). Usage: Hollow.Art.View <Pitch> [Yaw]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&View));

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Art.Dump"),
		TEXT("Prints the characters' cosmetic parts and the held tools on this machine."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}
