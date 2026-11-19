#include "Session/HollowSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HollowNetworkLog.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "Session/HollowNetworkSettings.h"
#include "Session/HollowSessionRules.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSessionSubsystem)

namespace HollowSessionSubsystemPrivate
{
	/** Travel option that turns the opened hub into a listen server. */
	const TCHAR* const ListenOption = TEXT("listen");
}

void UHollowSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		CreateCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateSessionComplete));
		DestroyCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));
		JoinCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinSessionComplete));
		InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleInviteAccepted));
	}
	else
	{
		UE_LOG(LogHollowNet, Warning, TEXT("No online session interface: hosting and invites are unavailable."));
	}

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
}

void UHollowSessionSubsystem::Deinitialize()
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
	}

	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	Super::Deinitialize();
}

bool UHollowSessionSubsystem::HostSession(const EHollowSessionPrivacy InPrivacy)
{
	if (State != EHollowSessionState::Idle)
	{
		ReportError(EHollowSessionError::AlreadyInSession, DescribeStatus());
		return false;
	}

	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		ReportError(EHollowSessionError::NoOnlineSubsystem, TEXT("No online session interface."));
		return false;
	}

	const int32 ControllerId = GetLocalControllerId();
	if (ControllerId == INDEX_NONE)
	{
		ReportError(EHollowSessionError::NoLocalPlayer, TEXT("No local player to host with."));
		return false;
	}

	const UHollowNetworkSettings* Settings = GetDefault<UHollowNetworkSettings>();
	if (Settings->HubMap.IsNull())
	{
		ReportError(EHollowSessionError::MissingMap, TEXT("HollowNetworkSettings.HubMap is not set (DefaultGame.ini)."));
		return false;
	}

	const bool bIsLanMatch = GetOnlineSubsystem()->GetSubsystemName() == NULL_SUBSYSTEM;
	Privacy = InPrivacy;
	bIsHost = true;
	SetState(EHollowSessionState::Creating);

	const FOnlineSessionSettings SessionSettings = HollowSessionRules::MakeSessionSettings(Privacy, Settings->MaxPlayers, bIsLanMatch);
	if (!Sessions->CreateSession(ControllerId, NAME_GameSession, SessionSettings))
	{
		bIsHost = false;
		SetState(EHollowSessionState::Idle);
		ReportError(EHollowSessionError::CreateFailed, TEXT("CreateSession rejected the request (a stale session may still exist)."));
		return false;
	}
	return true;
}

bool UHollowSessionSubsystem::ShowInviteUI() const
{
	const IOnlineSubsystem* Subsystem = GetOnlineSubsystem();
	const IOnlineExternalUIPtr ExternalUI = Subsystem ? Subsystem->GetExternalUIInterface() : nullptr;
	if (!IsHostingOnlineSession() || Privacy == EHollowSessionPrivacy::Solo || !ExternalUI.IsValid())
	{
		UE_LOG(LogHollowNet, Warning, TEXT("Invite UI unavailable (%s)."), *DescribeStatus());
		return false;
	}
	return ExternalUI->ShowInviteUI(GetLocalControllerId(), NAME_GameSession);
}

void UHollowSessionSubsystem::LeaveSession()
{
	EndSession(/*bTravelToFrontEnd=*/true);
}

bool UHollowSessionSubsystem::IsHostingOnlineSession() const
{
	return bIsHost && State == EHollowSessionState::InSession;
}

FString UHollowSessionSubsystem::DescribeStatus() const
{
	const IOnlineSubsystem* Subsystem = GetOnlineSubsystem();
	return FString::Printf(TEXT("State=%s Privacy=%s Role=%s OnlineSubsystem=%s"),
		*HollowEnumToString(State),
		*HollowEnumToString(Privacy),
		bIsHost ? TEXT("Host") : TEXT("Client"),
		Subsystem ? *Subsystem->GetSubsystemName().ToString() : TEXT("None"));
}

IOnlineSubsystem* UHollowSessionSubsystem::GetOnlineSubsystem() const
{
	// World-aware lookup keeps PIE instances on separate Null subsystems; outside PIE it is the default (Steam).
	return Online::GetSubsystem(GetGameInstance()->GetWorld());
}

IOnlineSessionPtr UHollowSessionSubsystem::GetSessionInterface() const
{
	const IOnlineSubsystem* Subsystem = GetOnlineSubsystem();
	return Subsystem ? Subsystem->GetSessionInterface() : nullptr;
}

int32 UHollowSessionSubsystem::GetLocalControllerId() const
{
	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	return LocalPlayer ? LocalPlayer->GetControllerId() : INDEX_NONE;
}

void UHollowSessionSubsystem::SetState(const EHollowSessionState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	UE_LOG(LogHollowNet, Display, TEXT("Session: %s"), *DescribeStatus());
	OnStateChanged.Broadcast(State);
}

void UHollowSessionSubsystem::ReportError(const EHollowSessionError Error, const FString& Details)
{
	UE_LOG(LogHollowNet, Warning, TEXT("Session error %s: %s"), *HollowEnumToString(Error), *Details);
	OnError.Broadcast(Error, Details);
}

void UHollowSessionSubsystem::JoinInvitedSession(const FOnlineSessionSearchResult& Invite)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	const int32 ControllerId = GetLocalControllerId();
	if (!Sessions.IsValid() || ControllerId == INDEX_NONE)
	{
		ReportError(EHollowSessionError::JoinFailed, TEXT("No session interface or local player."));
		return;
	}

	Privacy = Invite.Session.SessionSettings.bAllowJoinViaPresenceFriendsOnly ? EHollowSessionPrivacy::FriendsOnly : EHollowSessionPrivacy::InviteOnly;
	bIsHost = false;
	SetState(EHollowSessionState::Joining);
	if (!Sessions->JoinSession(ControllerId, NAME_GameSession, Invite))
	{
		SetState(EHollowSessionState::Idle);
		ReportError(EHollowSessionError::JoinFailed, TEXT("JoinSession rejected the request."));
	}
}

void UHollowSessionSubsystem::EndSession(const bool bTravelToFrontEnd)
{
	if (State == EHollowSessionState::Leaving)
	{
		return;
	}

	bTravelAfterEnd = bTravelToFrontEnd;
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr)
	{
		SetState(EHollowSessionState::Leaving);
		if (Sessions->DestroySession(NAME_GameSession))
		{
			return;   // completes in HandleDestroySessionComplete
		}
	}
	FinishEndSession();
}

void UHollowSessionSubsystem::FinishEndSession()
{
	bIsHost = false;
	SetState(EHollowSessionState::Idle);

	if (PendingInvite.IsSet())
	{
		const FOnlineSessionSearchResult Invite = MoveTemp(PendingInvite.GetValue());
		PendingInvite.Reset();
		JoinInvitedSession(Invite);
		return;
	}

	if (bTravelAfterEnd)
	{
		const TSoftObjectPtr<UWorld> FrontEndMap = GetDefault<UHollowNetworkSettings>()->FrontEndMap;
		if (FrontEndMap.IsNull())
		{
			ReportError(EHollowSessionError::MissingMap, TEXT("HollowNetworkSettings.FrontEndMap is not set (DefaultGame.ini)."));
			return;
		}
		UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), FrontEndMap);
	}
}

void UHollowSessionSubsystem::HandleCreateSessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession || State != EHollowSessionState::Creating)
	{
		return;
	}

	if (!bWasSuccessful)
	{
		bIsHost = false;
		SetState(EHollowSessionState::Idle);
		ReportError(EHollowSessionError::CreateFailed, TEXT("The online subsystem failed to create the session."));
		return;
	}

	SetState(EHollowSessionState::InSession);
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), GetDefault<UHollowNetworkSettings>()->HubMap,
		/*bAbsolute=*/true, HollowSessionSubsystemPrivate::ListenOption);
}

void UHollowSessionSubsystem::HandleDestroySessionComplete(const FName SessionName, const bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession || State != EHollowSessionState::Leaving)
	{
		return;
	}

	if (!bWasSuccessful)
	{
		UE_LOG(LogHollowNet, Warning, TEXT("DestroySession reported failure; continuing as if the session ended."));
	}
	FinishEndSession();
}

void UHollowSessionSubsystem::HandleJoinSessionComplete(const FName SessionName, const EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != NAME_GameSession || State != EHollowSessionState::Joining)
	{
		return;
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		SetState(EHollowSessionState::Idle);
		ReportError(EHollowSessionError::JoinFailed, LexToString(Result));
		return;
	}

	FString ConnectString;
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (!Sessions.IsValid() || !PlayerController || !Sessions->GetResolvedConnectString(SessionName, ConnectString))
	{
		ReportError(EHollowSessionError::ConnectStringUnavailable, TEXT("Could not resolve the host address."));
		EndSession(/*bTravelToFrontEnd=*/false);
		return;
	}

	SetState(EHollowSessionState::InSession);
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UHollowSessionSubsystem::HandleInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId,
	const FOnlineSessionSearchResult& Invite)
{
	if (!bWasSuccessful || !Invite.IsValid())
	{
		ReportError(EHollowSessionError::InviteInvalid, TEXT("The invite could not be resolved."));
		return;
	}

	// Early, friendly check; the network version handshake stays authoritative (ADR-0006).
	const int32 LocalBuildId = GetBuildUniqueId();
	if (Invite.Session.SessionSettings.BuildUniqueId != LocalBuildId)
	{
		ReportError(EHollowSessionError::IncompatibleBuild, FString::Printf(TEXT("Host build %d, local build %d."),
			Invite.Session.SessionSettings.BuildUniqueId, LocalBuildId));
		return;
	}

	if (State == EHollowSessionState::Idle)
	{
		JoinInvitedSession(Invite);
		return;
	}

	// Already hosting or playing: leave first, join once the old session is destroyed.
	PendingInvite = Invite;
	EndSession(/*bTravelToFrontEnd=*/false);
}

void UHollowSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, const ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	if (World && World->GetGameInstance() != GetGameInstance())
	{
		return;   // another PIE instance
	}

	ReportError(EHollowSessionError::NetworkFailure, FString::Printf(TEXT("%s: %s"), ENetworkFailure::ToString(FailureType), *ErrorString));

	// Clients lose the session when the connection drops; the engine already returns them to the default map.
	const bool bHostCannotListen = FailureType == ENetworkFailure::NetDriverListenFailure || FailureType == ENetworkFailure::NetDriverCreateFailure;
	if (State != EHollowSessionState::Idle && (!bIsHost || bHostCannotListen))
	{
		EndSession(/*bTravelToFrontEnd=*/false);
	}
}

void UHollowSessionSubsystem::HandleTravelFailure(UWorld* World, const ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World && World->GetGameInstance() != GetGameInstance())
	{
		return;   // another PIE instance
	}

	ReportError(EHollowSessionError::TravelFailure, FString::Printf(TEXT("%s: %s"), ETravelFailure::ToString(FailureType), *ErrorString));
	if (State != EHollowSessionState::Idle && !bIsHost)
	{
		EndSession(/*bTravelToFrontEnd=*/false);
	}
}
