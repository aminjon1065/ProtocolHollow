#pragma once

#include "Extraction/HollowRaidTypes.h"
#include "GameFramework/PlayerController.h"

#include "HollowPlayerController.generated.h"

class UHollowVoiceComponent;

/**
 * Thin player controller (CLAUDE.md sec. 8): client-to-server requests that belong to no pawn, and the player's voice
 * (S4-07: push-to-talk works alive, dead and in the hub, so it lives here rather than on the ExoFrame).
 */
UCLASS()
class HOLLOWGAME_API AHollowPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	explicit AHollowPlayerController(const FObjectInitializer& ObjectInitializer);

	UHollowVoiceComponent* GetVoice() const { return Voice; }

	/**
	 * This machine has assembled sector build BuildId and its rebuilt layout hashes to LayoutHash (ADR-0003). Soft
	 * validation like every Hollow RPC: a wrong hash gets the player kicked by the raid, never the server crashed.
	 */
	UFUNCTION(Server, Reliable)
	void Server_ReportSectorReady(int32 BuildId, uint32 LayoutHash);

	/** The host's result of this player's raid (ADR-0005): applied to the local profile, which shows the debrief. */
	UFUNCTION(Client, Reliable)
	void Client_ReceiveRaidResult(const FHollowRaidResult& Result);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void HandlePushToTalkPressed();
	void HandlePushToTalkReleased();

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Voice")
	TObjectPtr<UHollowVoiceComponent> Voice;
};
