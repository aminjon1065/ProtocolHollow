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
			const UPrimitiveComponent* Visual = Carryable->GetVisual();
			UE_LOG(LogHollowPhysics, Display, TEXT("Hollow.Carry.Dump: %s %s by %s%s%s serial %d, shown %s, at %s, visual at %s, simulating %d, rep movement %d, dormancy %s, attached to %s"),
				*It->GetName(), *HollowEnumToString(State.Mode), *GetNameSafe(State.Primary), State.Secondary ? TEXT(" and ") : TEXT(""),
				State.Secondary ? *GetNameSafe(State.Secondary) : TEXT(""), State.TransitionSerial,
				*HollowEnumToString(Carryable->GetPresentation()), *It->GetActorLocation().ToCompactString(),
				Visual ? *Visual->GetComponentLocation().ToCompactString() : TEXT("none"), Body->IsSimulatingPhysics(),
				It->IsReplicatingMovement(), *HollowEnumToString(It->NetDormancy.GetValue()), *GetNameSafe(Body->GetAttachParent()));
		}
		if (World->GetNetMode() != NM_Client)
		{
			// The server's view of every carrier: whose position it takes from the client (co-op carry, ADR-0028).
			for (TActorIterator<ACharacter> It(World); It; ++It)
			{
				const UHollowCharacterMovementComponent* Movement = Cast<UHollowCharacterMovementComponent>(It->GetCharacterMovement());
				if (Movement && It->FindComponentByClass<UHollowCarrierComponent>())
				{
					UE_LOG(LogHollowPhysics, Display, TEXT("Hollow.Carry.Dump: server sees %s at %s, server carry %s, client-authoritative %d, returning %d"),
						*It->GetName(), *It->GetActorLocation().ToCompactString(), *HollowEnumToString(Movement->GetServerCarry().Current),
						Movement->IsClientAuthoritative(), Movement->IsReturningAuthority());
				}
			}
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
		UE_LOG(LogHollowPhysics, Display, TEXT("Hollow.Carry.Dump: pawn %s at %s carrying %s (pending %s) with %s, claim %s, effective %s, max speed %.0f, speed %.0f, jump %d, corrections %d"),
			*Character->GetName(), *Character->GetActorLocation().ToCompactString(), Carrier->GetCarried() ? *GetNameSafe(Carrier->GetCarried()->GetOwner()) : TEXT("nothing"),
			Carrier->GetPendingPickup() ? *GetNameSafe(Carrier->GetPendingPickup()->GetOwner()) : TEXT("none"),
			Carrier->GetPartner() ? *GetNameSafe(Carrier->GetPartner()) : TEXT("nobody"), *HollowEnumToString(Movement->GetClaimedCarry()),
			*HollowEnumToString(Movement->GetEffectiveCarry()), Movement->GetMaxSpeed(), Movement->Velocity.Size2D(), Character->CanJump(),
			Movement->GetCorrectionCount());
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Carry.Dump"),
		TEXT("Prints this machine's carryables (carry state, how they are shown, physics, movement replication, dormancy), the server's carriers (client authority) and the local carrier's claim, partner, speed and corrections."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
