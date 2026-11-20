// Development console commands for heavy carrying (headless and multi-process checks). Never shipped.
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowPhysicsLog.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Util/HollowEnumUtils.h"

#if !UE_BUILD_SHIPPING

namespace HollowCarryDevConsole
{
	/** Prints this machine's carryables (state, presentation, physics, replication) and the local carrier's movement. */
	void Dump(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const UHollowCarryableComponent* Carryable = It->FindComponentByClass<UHollowCarryableComponent>();
			const UPrimitiveComponent* Body = Carryable ? Carryable->GetBody() : nullptr;
			if (!Body)
			{
				continue;
			}
			const FHollowCarryNetState& State = Carryable->GetCarryState();
			UE_LOG(LogHollowPhysics, Display, TEXT("Hollow.Carry.Dump: %s %s by %s serial %d, shown %s, at %s, simulating %d, rep movement %d, dormancy %s, attached to %s"),
				*It->GetName(), *HollowEnumToString(State.Mode), *GetNameSafe(State.Primary), State.TransitionSerial,
				*HollowEnumToString(Carryable->GetPresentation()), *It->GetActorLocation().ToCompactString(), Body->IsSimulatingPhysics(),
				It->IsReplicatingMovement(), *HollowEnumToString(It->NetDormancy.GetValue()), *GetNameSafe(Body->GetAttachParent()));
		}

		const UGameInstance* GameInstance = World->GetGameInstance();
		const APlayerController* Player = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
		const ACharacter* Character = Player ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
		const UHollowCharacterMovementComponent* Movement = Character ? Cast<UHollowCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
		const UHollowCarrierComponent* Carrier = Character ? Character->FindComponentByClass<UHollowCarrierComponent>() : nullptr;
		if (!Movement || !Carrier)
		{
			UE_LOG(LogHollowPhysics, Display, TEXT("Hollow.Carry.Dump: no local carrier."));
			return;
		}
		UE_LOG(LogHollowPhysics, Display, TEXT("Hollow.Carry.Dump: pawn %s at %s carrying %s (pending %s), claim %s, effective %s, max speed %.0f, speed %.0f, jump %d"),
			*Character->GetName(), *Character->GetActorLocation().ToCompactString(), Carrier->GetCarried() ? *GetNameSafe(Carrier->GetCarried()->GetOwner()) : TEXT("nothing"),
			Carrier->GetPendingPickup() ? *GetNameSafe(Carrier->GetPendingPickup()->GetOwner()) : TEXT("none"), *HollowEnumToString(Movement->GetClaimedCarry()),
			*HollowEnumToString(Movement->GetEffectiveCarry()), Movement->GetMaxSpeed(), Movement->Velocity.Size2D(), Character->CanJump());
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Carry.Dump"),
		TEXT("Prints this machine's carryables (carry state, how they are shown, physics, movement replication, dormancy) and the local carrier's claim and speed."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
