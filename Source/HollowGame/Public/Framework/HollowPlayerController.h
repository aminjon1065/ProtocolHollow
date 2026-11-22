#pragma once

#include "Extraction/HollowRaidTypes.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/HollowLoadoutRules.h"

#include "HollowPlayerController.generated.h"

class AHollowHubTerminal;
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

	/** The server validated this player's use of a hub terminal (ADR-0032): this machine opens it (UHollowHubTerminalSubsystem). */
	UFUNCTION(Client, Reliable)
	void Client_OpenTerminal(AHollowHubTerminal* Terminal);

	/** The host picks the squad's contract (ADR-0032); anyone else is refused softly. */
	UFUNCTION(Server, Reliable)
	void Server_SelectContract(const FPrimaryAssetId& Contract);

	/**
	 * This player plays Class now (S6-01, ADR-0034): its profile changed class at the hub's locker. Accepted in the
	 * hub only (a raid takes the class from the loadout snapshot); anything else is refused softly.
	 */
	UFUNCTION(Server, Reliable)
	void Server_SelectClass(const FPrimaryAssetId& Class);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void HandlePushToTalkPressed();
	void HandlePushToTalkReleased();

	/** In the hub, on this machine: the server hears the profile's class whenever it differs from the last one sent. */
	void SyncClassWithProfile();

	/** The class last sent to the server (none yet on a new map). */
	FPrimaryAssetId RequestedClass;
	FDelegateHandle ProfileHandle;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Voice")
	TObjectPtr<UHollowVoiceComponent> Voice;
};
