#include "Session/HollowSessionRules.h"

namespace HollowSessionRules
{
	FOnlineSessionSettings MakeSessionSettings(const EHollowSessionPrivacy Privacy, const int32 MaxPlayers, const bool bIsLanMatch)
	{
		const bool bSolo = Privacy == EHollowSessionPrivacy::Solo;
		const bool bFriendsOnly = Privacy == EHollowSessionPrivacy::FriendsOnly;

		FOnlineSessionSettings Settings;   // BuildUniqueId is filled by the engine (network-compatible changelist)
		Settings.NumPublicConnections = bSolo ? 1 : FMath::Max(1, MaxPlayers);
		Settings.NumPrivateConnections = 0;
		Settings.bIsLANMatch = bIsLanMatch;
		Settings.bIsDedicated = false;
		Settings.bUsesStats = false;
		Settings.bAntiCheatProtected = false;
		Settings.bUsesPresence = true;
		Settings.bUseLobbiesIfAvailable = true;
		Settings.bUseLobbiesVoiceChatIfAvailable = false;   // proximity voice goes through UE VOIP (ADR-0006)
		Settings.bAllowJoinInProgress = !bSolo;             // phase gating is done by PreLogin, not by the lobby
		Settings.bShouldAdvertise = !bSolo;
		Settings.bAllowInvites = !bSolo;
		Settings.bAllowJoinViaPresence = bFriendsOnly;
		Settings.bAllowJoinViaPresenceFriendsOnly = bFriendsOnly;
		return Settings;
	}

	EHollowJoinRejection ValidateJoin(const FHollowJoinContext& Context)
	{
		if (Context.ConnectedPlayers >= Context.MaxPlayers)
		{
			return EHollowJoinRejection::SessionFull;
		}
		if (!Context.bHostsOnlineSession)
		{
			return EHollowJoinRejection::None;
		}
		if (Context.Privacy == EHollowSessionPrivacy::Solo)
		{
			return EHollowJoinRejection::SoloSession;
		}
		if (Context.Phase != EHollowSessionPhase::Hub)
		{
			return EHollowJoinRejection::RaidInProgress;
		}
		return EHollowJoinRejection::None;
	}

	FString GetRejectionMessage(const EHollowJoinRejection Rejection)
	{
		switch (Rejection)
		{
		case EHollowJoinRejection::SessionFull:    return TEXT("The squad is full.");
		case EHollowJoinRejection::SoloSession:    return TEXT("The host is playing solo.");
		case EHollowJoinRejection::RaidInProgress: return TEXT("The squad is on a raid. Join from the hub.");
		case EHollowJoinRejection::None:           break;
		}
		return FString();
	}
}
