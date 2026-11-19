#include "Framework/HollowGameModeBase.h"

#include "Engine/GameInstance.h"
#include "HollowGameLog.h"
#include "Session/HollowNetworkSettings.h"
#include "Session/HollowSessionRules.h"
#include "Session/HollowSessionSubsystem.h"
#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGameModeBase)

void AHollowGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UHollowSessionSubsystem* Sessions = GameInstance ? GameInstance->GetSubsystem<UHollowSessionSubsystem>() : nullptr;

	FHollowJoinContext Context;
	Context.bHostsOnlineSession = Sessions && Sessions->IsHostingOnlineSession();
	Context.Privacy = Sessions ? Sessions->GetPrivacy() : EHollowSessionPrivacy::Solo;
	Context.Phase = SessionPhase;
	Context.ConnectedPlayers = GetNumPlayers();
	Context.MaxPlayers = GetDefault<UHollowNetworkSettings>()->MaxPlayers;

	const EHollowJoinRejection Rejection = HollowSessionRules::ValidateJoin(Context);
	if (Rejection != EHollowJoinRejection::None)
	{
		ErrorMessage = HollowSessionRules::GetRejectionMessage(Rejection);
		UE_LOG(LogHollowGame, Log, TEXT("PreLogin rejected %s: %s (%s)"), *Address, *HollowEnumToString(Rejection), *ErrorMessage);
	}
}
