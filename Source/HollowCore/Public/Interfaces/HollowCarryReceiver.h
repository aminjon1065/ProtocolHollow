#pragma once

#include "UObject/Interface.h"

#include "HollowCarryReceiver.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowCarryReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * An interactable a player hands their load to (S7-02, ADR-0039: the Purification Altar takes an Aether core). A
 * player carrying with both hands interacts with nothing else (UHollowInteractionSettings::BlockingTags): a receiver is
 * focused all the same, and Interact there hands the load over instead of dropping it. Whether it takes this load now
 * is its IHollowInteractable::CanInteract. C++ only (CLAUDE.md sec. 1).
 */
class HOLLOWCORE_API IHollowCarryReceiver
{
	GENERATED_BODY()
};
