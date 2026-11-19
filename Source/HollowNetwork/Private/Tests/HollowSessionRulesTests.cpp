#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Session/HollowSessionRules.h"

namespace HollowSessionRulesTests
{
	constexpr int32 SquadSize = 4;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSessionSettingsByPrivacyTest, "Hollow.Net.Session.SettingsByPrivacy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSessionSettingsByPrivacyTest::RunTest(const FString& Parameters)
{
	using namespace HollowSessionRulesTests;

	// Common to every privacy: Steam lobby + presence, listen server, no stats/anti-cheat, engine build id.
	for (const EHollowSessionPrivacy Privacy : { EHollowSessionPrivacy::Solo, EHollowSessionPrivacy::InviteOnly, EHollowSessionPrivacy::FriendsOnly })
	{
		const FOnlineSessionSettings Settings = HollowSessionRules::MakeSessionSettings(Privacy, SquadSize, /*bIsLanMatch=*/false);
		const FString Name = UEnum::GetValueAsString(Privacy);
		TestTrue(Name + TEXT(" uses Steam lobbies"), Settings.bUseLobbiesIfAvailable);
		TestTrue(Name + TEXT(" uses presence"), Settings.bUsesPresence);
		TestFalse(Name + TEXT(" is a listen server"), Settings.bIsDedicated);
		TestFalse(Name + TEXT(" is not a LAN match on Steam"), Settings.bIsLANMatch);
		TestEqual(Name + TEXT(" has no private slots"), Settings.NumPrivateConnections, 0);
		TestEqual(Name + TEXT(" carries the engine build id"), Settings.BuildUniqueId, GetBuildUniqueId());
	}

	const FOnlineSessionSettings Solo = HollowSessionRules::MakeSessionSettings(EHollowSessionPrivacy::Solo, SquadSize, false);
	TestEqual(TEXT("Solo keeps a single slot"), Solo.NumPublicConnections, 1);
	TestFalse(TEXT("Solo is not advertised"), Solo.bShouldAdvertise);
	TestFalse(TEXT("Solo allows no invites"), Solo.bAllowInvites);
	TestFalse(TEXT("Solo allows no presence joins"), Solo.bAllowJoinViaPresence);

	// Steam: advertise + invites without presence join -> private (invite-only) lobby.
	const FOnlineSessionSettings Invite = HollowSessionRules::MakeSessionSettings(EHollowSessionPrivacy::InviteOnly, SquadSize, false);
	TestEqual(TEXT("InviteOnly has a squad of slots"), Invite.NumPublicConnections, SquadSize);
	TestTrue(TEXT("InviteOnly is advertised to the lobby system"), Invite.bShouldAdvertise);
	TestTrue(TEXT("InviteOnly allows invites"), Invite.bAllowInvites);
	TestFalse(TEXT("InviteOnly allows no presence joins"), Invite.bAllowJoinViaPresence);
	TestFalse(TEXT("InviteOnly is not friends-only"), Invite.bAllowJoinViaPresenceFriendsOnly);

	// Steam: advertise + friends-only presence -> friends-only lobby.
	const FOnlineSessionSettings Friends = HollowSessionRules::MakeSessionSettings(EHollowSessionPrivacy::FriendsOnly, SquadSize, false);
	TestEqual(TEXT("FriendsOnly has a squad of slots"), Friends.NumPublicConnections, SquadSize);
	TestTrue(TEXT("FriendsOnly allows invites"), Friends.bAllowInvites);
	TestTrue(TEXT("FriendsOnly allows presence joins"), Friends.bAllowJoinViaPresence);
	TestTrue(TEXT("FriendsOnly restricts presence joins to friends"), Friends.bAllowJoinViaPresenceFriendsOnly);

	TestTrue(TEXT("Null subsystem sessions are LAN matches"),
		HollowSessionRules::MakeSessionSettings(EHollowSessionPrivacy::InviteOnly, SquadSize, /*bIsLanMatch=*/true).bIsLANMatch);
	TestEqual(TEXT("A non-positive squad size still leaves the host slot"),
		HollowSessionRules::MakeSessionSettings(EHollowSessionPrivacy::InviteOnly, 0, false).NumPublicConnections, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSessionJoinGateTest, "Hollow.Net.Session.JoinGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSessionJoinGateTest::RunTest(const FString& Parameters)
{
	using namespace HollowSessionRulesTests;

	auto MakeContext = [](const bool bOnline, const EHollowSessionPrivacy Privacy, const EHollowSessionPhase Phase, const int32 Connected)
	{
		FHollowJoinContext Context;
		Context.bHostsOnlineSession = bOnline;
		Context.Privacy = Privacy;
		Context.Phase = Phase;
		Context.ConnectedPlayers = Connected;
		Context.MaxPlayers = SquadSize;
		return Context;
	};

	using EPrivacy = EHollowSessionPrivacy;
	using EPhase = EHollowSessionPhase;
	using EReject = EHollowJoinRejection;

	TestTrue(TEXT("Invite session in the hub accepts the 2nd player"),
		HollowSessionRules::ValidateJoin(MakeContext(true, EPrivacy::InviteOnly, EPhase::Hub, 1)) == EReject::None);
	TestTrue(TEXT("Friends session in the hub accepts the 4th player"),
		HollowSessionRules::ValidateJoin(MakeContext(true, EPrivacy::FriendsOnly, EPhase::Hub, SquadSize - 1)) == EReject::None);
	TestTrue(TEXT("A full squad rejects the 5th player"),
		HollowSessionRules::ValidateJoin(MakeContext(true, EPrivacy::InviteOnly, EPhase::Hub, SquadSize)) == EReject::SessionFull);
	TestTrue(TEXT("Solo sessions reject everyone"),
		HollowSessionRules::ValidateJoin(MakeContext(true, EPrivacy::Solo, EPhase::Hub, 1)) == EReject::SoloSession);
	TestTrue(TEXT("Vertical slice: no joins during a raid (ADR-0006)"),
		HollowSessionRules::ValidateJoin(MakeContext(true, EPrivacy::InviteOnly, EPhase::Raid, 1)) == EReject::RaidInProgress);
	TestTrue(TEXT("Direct connections without a session (PIE) may start in a raid"),
		HollowSessionRules::ValidateJoin(MakeContext(false, EPrivacy::Solo, EPhase::Raid, 1)) == EReject::None);
	TestTrue(TEXT("Capacity applies even without a session"),
		HollowSessionRules::ValidateJoin(MakeContext(false, EPrivacy::Solo, EPhase::Hub, SquadSize)) == EReject::SessionFull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSessionRejectionMessagesTest, "Hollow.Net.Session.RejectionMessages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSessionRejectionMessagesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("No message when the join is accepted"), HollowSessionRules::GetRejectionMessage(EHollowJoinRejection::None).IsEmpty());
	for (const EHollowJoinRejection Rejection : { EHollowJoinRejection::SessionFull, EHollowJoinRejection::SoloSession, EHollowJoinRejection::RaidInProgress })
	{
		TestFalse(UEnum::GetValueAsString(Rejection) + TEXT(" explains the rejection"), HollowSessionRules::GetRejectionMessage(Rejection).IsEmpty());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
