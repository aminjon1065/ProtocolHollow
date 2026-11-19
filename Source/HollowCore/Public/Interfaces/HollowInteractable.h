#pragma once

#include "UObject/Interface.h"

#include "HollowInteractable.generated.h"

/** What an interactable shows to the player who focuses it (local, cosmetic). */
USTRUCT(BlueprintType)
struct FHollowInteractionInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText Prompt;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something a player can interact with: switches, doors, cores, terminals. C++ only (CLAUDE.md sec. 1).
 * The interacting player's UHollowInteractionComponent focuses it locally and asks the server, which validates the
 * request (range, line of sight, rate, state) before calling Interact.
 */
class HOLLOWCORE_API IHollowInteractable
{
	GENERATED_BODY()

public:
	/** Display data for Interactor. Called locally, every focus change. */
	virtual FHollowInteractionInfo GetInteractionInfo(const AActor* Interactor) const = 0;

	/** Whether Interactor may interact right now. Checked locally for focus and again on the server. */
	virtual bool CanInteract(const AActor* Interactor) const = 0;

	/** Performs the interaction. Server only, after validation. */
	virtual void Interact(AActor* Interactor) = 0;
};
