#pragma once

#include "GameFramework/Actor.h"
#include "Interfaces/HollowInteractable.h"

#include "HollowHubTerminal.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** What a hub terminal opens (ADR-0032). */
UENUM(BlueprintType)
enum class EHollowTerminalKind : uint8
{
	/** The host picks the raid's contract and launches it; the others see the choice. */
	Contracts,
	/** The shop, the workbench and the ship's upgrades (ADR-0031): each player's own profile. */
	Workbench,
	/** The loadout locker: stash <-> loadout, insurance. */
	Locker,
	/** NAR-01b (ADR-0050): the crew archive, the Codex of the crew logs found: each player's own profile. */
	Codex
};

/**
 * A terminal of the hub shuttle Skiff (ADR-0032). Interacting (E) goes the way of every interactable: the server checks
 * range and line of sight, then tells only that player's machine to open the terminal
 * (AHollowPlayerController::Client_OpenTerminal -> UHollowHubTerminalSubsystem). What it opens is the player's own,
 * local profile; nothing of it is replicated. The screens come with S5-04. Level-placed, dormant, never ticks.
 */
UCLASS()
class HOLLOWGAME_API AHollowHubTerminal : public AActor, public IHollowInteractable
{
	GENERATED_BODY()

public:
	explicit AHollowHubTerminal(const FObjectInitializer& ObjectInitializer);

	EHollowTerminalKind GetKind() const { return Kind; }

	/** Where a player stands to use it: UseDistance in front of it, on its floor. */
	FVector GetUseLocation() const;

	/** Whether Interactor's machine decides what only the host decides (the listen server or a solo game). */
	static bool IsHostSide(const AActor* Interactor);

	//~ IHollowInteractable
	virtual FHollowInteractionInfo GetInteractionInfo(const AActor* Interactor) const override;
	virtual bool CanInteract(const AActor* Interactor) const override;
	virtual void Interact(AActor* Interactor) override;

protected:
	/** On the floor, facing the way its user looks at it from (its X axis points at the user). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Terminal")
	TObjectPtr<USceneComponent> Base;

	/** What is focused and blocks the way (greybox; the art pass replaces the meshes). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Terminal")
	TObjectPtr<UStaticMeshComponent> Cabinet;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Terminal")
	TObjectPtr<UStaticMeshComponent> Screen;

	UPROPERTY(EditAnywhere, Category = "Hollow|Terminal")
	EHollowTerminalKind Kind = EHollowTerminalKind::Workbench;

	UPROPERTY(EditAnywhere, Category = "Hollow|Terminal")
	FText Prompt;

	/** Shown instead of Prompt to a player who only watches what the host decides (the contracts terminal); empty: Prompt. */
	UPROPERTY(EditAnywhere, Category = "Hollow|Terminal")
	FText GuestPrompt;

	/** How far in front of the terminal its user stands. */
	UPROPERTY(EditAnywhere, Category = "Hollow|Terminal", meta = (ClampMin = "0", Units = "Centimeters"))
	float UseDistance = 0.f;

	friend class FHollowHubTerminalsTest;   // places terminals of each kind without a level
};
