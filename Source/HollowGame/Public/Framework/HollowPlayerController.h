#pragma once

#include "Extraction/HollowRaidTypes.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/HollowLoadoutRules.h"

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

	/** The raid asks what this player takes into it (ADR-0030): answered from the local profile by Server_SubmitLoadout. */
	UFUNCTION(Client, Reliable)
	void Client_RequestLoadout(const FGuid& RaidId);

	/**
	 * What this player takes into raid Loadout.RaidId (ADR-0005, ADR-0030). Checked by the host softly, like every Hollow
	 * RPC: what the rules refuse is dropped, nobody is kicked.
	 */
	UFUNCTION(Server, Reliable)
	void Server_SubmitLoadout(const FHollowLoadoutSnapshot& Loadout);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void HandlePushToTalkPressed();
	void HandlePushToTalkReleased();

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Voice")
	TObjectPtr<UHollowVoiceComponent> Voice;
};
