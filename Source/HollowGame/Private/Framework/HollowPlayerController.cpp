#include "Framework/HollowPlayerController.h"

#include "Engine/World.h"
#include "Framework/HollowRaidGameMode.h"
#include "HollowGameLog.h"
#include "Sector/HollowSectorAuthorityComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPlayerController)

void AHollowPlayerController::Server_ReportSectorReady_Implementation(const int32 BuildId, const uint32 LayoutHash)
{
	const AHollowRaidGameMode* Raid = GetWorld()->GetAuthGameMode<AHollowRaidGameMode>();
	if (!Raid)
	{
		UE_LOG(LogHollowGame, Verbose, TEXT("%s reported sector build %d outside a raid; ignored."), *GetName(), BuildId);
		return;
	}
	Raid->GetSectorAuthority()->HandleReport(this, BuildId, LayoutHash);
}
