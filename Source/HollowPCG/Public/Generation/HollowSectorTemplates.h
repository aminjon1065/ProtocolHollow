#pragma once

#include "Generation/HollowSectorGenTypes.h"

class UHollowRoomDefinition;

/** Bridge from room data assets to the pure generator: everything UObject-related happens before Generate. */
namespace HollowSectorTemplates
{
	/** Role of a room under the rules, from its Hollow.Room.Type.* tag; unset if the rules do not use that type. */
	HOLLOWPCG_API TOptional<EHollowRoomRole> ResolveRole(const FGameplayTagContainer& RoomTags, const FHollowSectorGenRules& Rules);

	/** Plain template of a room definition; unset if the room has no role under the rules. */
	HOLLOWPCG_API TOptional<FHollowRoomTemplate> MakeTemplate(const UHollowRoomDefinition& Room, const FHollowSectorGenRules& Rules);

	/** Templates of every room that has a role (input order does not matter: the generator sorts by id). */
	HOLLOWPCG_API TArray<FHollowRoomTemplate> MakeTemplates(TConstArrayView<const UHollowRoomDefinition*> Rooms, const FHollowSectorGenRules& Rules);
}
