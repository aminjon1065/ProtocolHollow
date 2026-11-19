#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Session/HollowSessionTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "HollowSessionSubsystem.generated.h"

class IOnlineSubsystem;
class UNetDriver;

/**
 * Thin Steam (OSSv1) session layer (ADR-0006): host a listen session, accept invites, leave.
 * Solo and co-op share this path. In PIE the engine falls back to the Null subsystem (LAN sessions).
 * Session rules live in HollowSessionRules (pure, unit-tested).
 */
UCLASS()
class HOLLOWNETWORK_API UHollowSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowSessionStateChanged, EHollowSessionState /*NewState*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowSessionError, EHollowSessionError /*Error*/, const FString& /*Details*/);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Creates the online session, then opens the hub as a listen server. Returns false if the request was not started. */
	bool HostSession(EHollowSessionPrivacy InPrivacy);

	/** Opens the platform invite dialog (Steam overlay) for the current session. */
	bool ShowInviteUI() const;

	/** Leaves the session (ends it when hosting) and returns to the front-end map. */
	void LeaveSession();

	EHollowSessionState GetState() const { return State; }
	EHollowSessionPrivacy GetPrivacy() const { return Privacy; }

	/** True while this machine hosts an online session (gates PreLogin, see HollowSessionRules::ValidateJoin). */
	bool IsHostingOnlineSession() const;

	/** One-line status for the console and logs. */
	FString DescribeStatus() const;

	FOnHollowSessionStateChanged OnStateChanged;
	FOnHollowSessionError OnError;

private:
	IOnlineSubsystem* GetOnlineSubsystem() const;
	IOnlineSessionPtr GetSessionInterface() const;
	int32 GetLocalControllerId() const;

	void SetState(EHollowSessionState NewState);
	void ReportError(EHollowSessionError Error, const FString& Details);

	void JoinInvitedSession(const FOnlineSessionSearchResult& Invite);
	void EndSession(bool bTravelToFrontEnd);
	void FinishEndSession();

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Invite);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	EHollowSessionState State = EHollowSessionState::Idle;
	EHollowSessionPrivacy Privacy = EHollowSessionPrivacy::Solo;
	bool bIsHost = false;
	/** Whether the pending EndSession should open the front-end map once the session is destroyed. */
	bool bTravelAfterEnd = true;

	/** Invite accepted while another session was active: joined as soon as that session is destroyed. */
	TOptional<FOnlineSessionSearchResult> PendingInvite;

	FDelegateHandle CreateCompleteHandle;
	FDelegateHandle DestroyCompleteHandle;
	FDelegateHandle JoinCompleteHandle;
	FDelegateHandle InviteAcceptedHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
