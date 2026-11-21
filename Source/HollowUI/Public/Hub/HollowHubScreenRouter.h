#pragma once

#include "UObject/Object.h"

#include "HollowHubScreenRouter.generated.h"

class AHollowHubTerminal;
class APlayerController;
class UHollowHubScreenWidget;
class UHollowHubTerminalSubsystem;
class UHollowPrimaryLayout;
enum class EHollowTerminalKind : uint8;

/**
 * Terminal -> screen (S5-04, ADR-0033), for one local player: when the server opens a hub terminal for the player
 * (UHollowHubTerminalSubsystem::OnTerminalOpened) the screen of its kind goes on the Menu layer; a terminal closed
 * elsewhere takes its screen down, and leaving the screen closes the terminal. Owned by the HUD; tests drive it with
 * their own layout.
 */
UCLASS()
class HOLLOWUI_API UHollowHubScreenRouter : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UHollowPrimaryLayout* InLayout, APlayerController* InPlayer);
	void Deinitialize();

	/** The player's hub screen while it is shown. */
	UHollowHubScreenWidget* GetScreen() const;

	/** The screen class for a terminal kind (UHollowUISettings). */
	static TSubclassOf<UHollowHubScreenWidget> ScreenClass(EHollowTerminalKind Kind);

private:
	void HandleOpened(APlayerController* InPlayer, AHollowHubTerminal* Terminal);
	void HandleClosed(APlayerController* InPlayer, AHollowHubTerminal* Terminal);

	TWeakObjectPtr<UHollowPrimaryLayout> Layout;
	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<UHollowHubTerminalSubsystem> Terminals;

	UPROPERTY(Transient)
	TObjectPtr<UHollowHubScreenWidget> Screen;

	FDelegateHandle OpenedHandle;
	FDelegateHandle ClosedHandle;
};
