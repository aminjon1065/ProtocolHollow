#include "Menus/HollowMenuRules.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Localization/HollowLanguage.h"
#include "Raid/HollowRaidLauncher.h"
#include "Session/HollowNetworkSettings.h"
#include "Session/HollowSessionSubsystem.h"
#include "Settings/HollowGameUserSettings.h"

FHollowSystemMenuOptions HollowMenuRules::SystemMenuOptions(const FHollowMenuContext& Context)
{
	FHollowSystemMenuOptions Options;
	Options.bInvite = Context.bInSession && Context.bHost && Context.bOnline;
	Options.bStartRaid = Context.bInSession && Context.bHost && Context.bInHub;
	Options.bLeave = Context.bInSession;
	return Options;
}

FHollowMenuContext HollowMenuRules::ReadContext(const UWorld* World)
{
	FHollowMenuContext Context;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UHollowSessionSubsystem* Session = GameInstance ? GameInstance->GetSubsystem<UHollowSessionSubsystem>() : nullptr;
	Context.bInSession = Session && Session->GetState() == EHollowSessionState::InSession;
	Context.bHost = World && World->GetNetMode() != NM_Client;
	Context.bOnline = Session && Session->GetPrivacy() != EHollowSessionPrivacy::Solo;
	Context.bInHub = HollowRaidLauncher::CanStartRaid(World);
	return Context;
}

bool HollowMenuRules::IsFrontEnd(const UWorld* World)
{
	const FString FrontEnd = GetDefault<UHollowNetworkSettings>()->FrontEndMap.GetLongPackageName();
	return World && !FrontEnd.IsEmpty() && UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == FrontEnd;
}

EHollowLanguage HollowMenuRules::CycleLanguage()
{
	const EHollowLanguage Next = HollowLanguage::Next(HollowLanguage::GetApplied());
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		Settings->SetLanguage(Next);   // kept for the next launch
	}
	else
	{
		HollowLanguage::Apply(Next);
	}
	return HollowLanguage::GetApplied();
}
