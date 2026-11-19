#pragma once

#include "CoreMinimal.h"

#include "HollowSessionTypes.generated.h"

/** Who may join a hosted session (ADR-0006). Solo runs the same listen-server path as co-op (CLAUDE.md sec. 1). */
UENUM(BlueprintType)
enum class EHollowSessionPrivacy : uint8
{
	/** Listen server for one player: not advertised, no invites, every remote join is rejected. */
	Solo,
	/** Private Steam lobby: joinable only through invites. */
	InviteOnly,
	/** Friends-only Steam lobby: invites plus "Join Game" from the friends list. */
	FriendsOnly
};

/** Gameplay phase of the hosting game mode. The vertical slice accepts joins in the hub only (ADR-0006). */
UENUM(BlueprintType)
enum class EHollowSessionPhase : uint8
{
	Hub,
	Raid
};

UENUM(BlueprintType)
enum class EHollowSessionState : uint8
{
	Idle,
	Creating,
	Joining,
	InSession,
	Leaving
};

/** Why the host refused a connecting player (AHollowGameModeBase::PreLogin). */
UENUM(BlueprintType)
enum class EHollowJoinRejection : uint8
{
	None,
	SessionFull,
	SoloSession,
	RaidInProgress
};

UENUM(BlueprintType)
enum class EHollowSessionError : uint8
{
	NoOnlineSubsystem,
	NoLocalPlayer,
	AlreadyInSession,
	CreateFailed,
	MissingMap,
	InviteInvalid,
	IncompatibleBuild,
	JoinFailed,
	ConnectStringUnavailable,
	NetworkFailure,
	TravelFailure
};

/** Host-side snapshot used to decide whether a connecting player may join. Plain data: unit-testable. */
struct FHollowJoinContext
{
	/** False for direct connections without an online session (PIE, "open <ip>"): only capacity is enforced. */
	bool bHostsOnlineSession = false;
	EHollowSessionPrivacy Privacy = EHollowSessionPrivacy::Solo;
	EHollowSessionPhase Phase = EHollowSessionPhase::Hub;
	/** Players already in the game, host included. */
	int32 ConnectedPlayers = 0;
	int32 MaxPlayers = 0;
};
