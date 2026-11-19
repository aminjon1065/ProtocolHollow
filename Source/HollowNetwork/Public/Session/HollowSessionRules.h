#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Session/HollowSessionTypes.h"

/**
 * Pure session rules (no UWorld, no online subsystem): tested by Hollow.Net.Session.*.
 * Build compatibility is enforced by the engine (network version handshake + BuildUniqueId), see ADR-0006.
 */
namespace HollowSessionRules
{
	/**
	 * Online session settings for a hosted listen server. Steam maps them to lobby types:
	 * Solo -> private (not advertised), InviteOnly -> private, FriendsOnly -> friends-only.
	 */
	HOLLOWNETWORK_API FOnlineSessionSettings MakeSessionSettings(EHollowSessionPrivacy Privacy, int32 MaxPlayers, bool bIsLanMatch);

	/** Decides whether a connecting player may join. Capacity always applies; session gates only with an online session. */
	HOLLOWNETWORK_API EHollowJoinRejection ValidateJoin(const FHollowJoinContext& Context);

	/** Message returned to the rejected client through PreLogin. Empty for EHollowJoinRejection::None. */
	HOLLOWNETWORK_API FString GetRejectionMessage(EHollowJoinRejection Rejection);
}
