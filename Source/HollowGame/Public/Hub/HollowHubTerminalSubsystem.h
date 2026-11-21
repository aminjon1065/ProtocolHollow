#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "HollowHubTerminalSubsystem.generated.h"

class AHollowHubTerminal;
class APlayerController;

/**
 * Which hub terminal a local player has open, on this machine only (ADR-0032): told by the server
 * (AHollowPlayerController::Client_OpenTerminal) after a validated interaction; the hub's screens (S5-04) follow
 * OnTerminalOpened and close it again. Nothing here is replicated.
 */
UCLASS()
class HOLLOWGAME_API UHollowHubTerminalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowTerminal, APlayerController* /*Player*/, AHollowHubTerminal* /*Terminal*/);

	static UHollowHubTerminalSubsystem* Get(const UWorld* World);

	/** Player opened Terminal (another one open before closes first). */
	void Open(APlayerController* Player, AHollowHubTerminal* Terminal);

	/** Player's open terminal closes (its screen was left). */
	void Close(APlayerController* Player);

	/** Player's open terminal, if any. */
	AHollowHubTerminal* GetOpenTerminal(const APlayerController* Player) const;

	FOnHollowTerminal OnTerminalOpened;
	FOnHollowTerminal OnTerminalClosed;

private:
	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<AHollowHubTerminal>> OpenTerminals;
};
