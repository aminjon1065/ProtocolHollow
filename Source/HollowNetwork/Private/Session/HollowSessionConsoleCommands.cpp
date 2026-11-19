// Development console commands for session testing before the front-end UI exists (S2-06). Never shipped.
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HollowNetworkLog.h"
#include "Session/HollowSessionSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace HollowSessionConsole
{
	/** Privacy used by "Hollow.Session.Host" without arguments: the usual co-op case. */
	constexpr EHollowSessionPrivacy DefaultHostPrivacy = EHollowSessionPrivacy::InviteOnly;

	UHollowSessionSubsystem* FindSubsystem(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UHollowSessionSubsystem* Sessions = GameInstance ? GameInstance->GetSubsystem<UHollowSessionSubsystem>() : nullptr;
		if (!Sessions)
		{
			UE_LOG(LogHollowNet, Warning, TEXT("Hollow.Session.*: no game instance / session subsystem in this world."));
		}
		return Sessions;
	}

	void Host(const TArray<FString>& Args, UWorld* World)
	{
		UHollowSessionSubsystem* Sessions = FindSubsystem(World);
		if (!Sessions)
		{
			return;
		}

		EHollowSessionPrivacy Privacy = DefaultHostPrivacy;
		if (Args.Num() > 0)
		{
			const int64 Value = StaticEnum<EHollowSessionPrivacy>()->GetValueByNameString(Args[0]);
			if (Value == INDEX_NONE)
			{
				UE_LOG(LogHollowNet, Warning, TEXT("Unknown privacy '%s'. Use Solo, InviteOnly or FriendsOnly."), *Args[0]);
				return;
			}
			Privacy = static_cast<EHollowSessionPrivacy>(Value);
		}
		Sessions->HostSession(Privacy);
	}

	void Invite(UWorld* World)
	{
		if (UHollowSessionSubsystem* Sessions = FindSubsystem(World))
		{
			Sessions->ShowInviteUI();
		}
	}

	void Leave(UWorld* World)
	{
		if (UHollowSessionSubsystem* Sessions = FindSubsystem(World))
		{
			Sessions->LeaveSession();
		}
	}

	void Status(UWorld* World)
	{
		if (const UHollowSessionSubsystem* Sessions = FindSubsystem(World))
		{
			UE_LOG(LogHollowNet, Display, TEXT("Session: %s"), *Sessions->DescribeStatus());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs HostCommand(
		TEXT("Hollow.Session.Host"),
		TEXT("Hosts a listen session and opens the hub. Usage: Hollow.Session.Host [Solo|InviteOnly|FriendsOnly] (default InviteOnly)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Host));

	FAutoConsoleCommandWithWorld InviteCommand(
		TEXT("Hollow.Session.Invite"),
		TEXT("Opens the Steam overlay invite dialog for the hosted session."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Invite));

	FAutoConsoleCommandWithWorld LeaveCommand(
		TEXT("Hollow.Session.Leave"),
		TEXT("Leaves (or ends, when hosting) the session and returns to the front-end map."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Leave));

	FAutoConsoleCommandWithWorld StatusCommand(
		TEXT("Hollow.Session.Status"),
		TEXT("Prints the session state, privacy, role and online subsystem."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Status));
}

#endif // !UE_BUILD_SHIPPING
