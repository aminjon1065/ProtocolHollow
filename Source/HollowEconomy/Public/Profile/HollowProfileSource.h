#pragma once

#include "Crafting/HollowCraftingRules.h"
#include "Delegates/Delegate.h"

struct FHollowProfileState;

/**
 * Where the hub's screens read and change a profile (S5-04, ADR-0033): the local player's UHollowProfileSubsystem in
 * a game, a profile in memory in tests (the subsystem never exists in automation worlds, so tests never touch the
 * player's real profile). A plain C++ interface: a screen keeps no source, it asks for one when it needs it.
 */
class HOLLOWECONOMY_API IHollowProfileSource
{
public:
	using FOnProfileChanged = TMulticastDelegate<void()>;

	virtual ~IHollowProfileSource() = default;

	/** The profile with its escrow and remembered transactions (the screens preview actions on a copy). */
	virtual const FHollowProfileState& GetProfileState() const = 0;

	/** A transaction of the hub (ADR-0031) under HollowCraftingRules; announced through ProfileChanged when it applied. */
	virtual EHollowTransactionResult ApplyTransaction(const FHollowProfileTransaction& Transaction) = 0;

	/** Credits, stash, loadout or upgrades changed. */
	virtual FOnProfileChanged& ProfileChanged() = 0;
};
