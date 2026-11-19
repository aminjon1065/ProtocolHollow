#pragma once

#include "GameFramework/PlayerController.h"

#include "HollowPlayerController.generated.h"

/** Thin player controller (CLAUDE.md sec. 8): client-to-server requests that belong to no pawn. */
UCLASS()
class HOLLOWGAME_API AHollowPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/**
	 * This machine has assembled sector build BuildId and its rebuilt layout hashes to LayoutHash (ADR-0003). Soft
	 * validation like every Hollow RPC: a wrong hash gets the player kicked by the raid, never the server crashed.
	 */
	UFUNCTION(Server, Reliable)
	void Server_ReportSectorReady(int32 BuildId, uint32 LayoutHash);
};
