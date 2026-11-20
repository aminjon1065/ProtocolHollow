// Development console commands for bulkheads (headless and multi-process checks). Never shipped.
#include "Breach/HollowBreachableBulkhead.h"
#include "Breach/HollowStructuralIntegrityComponent.h"
#include "Destruction/HollowDestructionBudgetSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HollowPhysicsLog.h"
#include "NavAreas/NavArea.h"
#include "Scalability.h"
#include "Util/HollowEnumUtils.h"

#if !UE_BUILD_SHIPPING

namespace HollowBreachDevConsole
{
	/** Prints every bulkhead of this machine (state, zones and how each was shown, live debris, nav) and the budget. */
	void Dump(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AHollowBreachableBulkhead> It(World); It; ++It)
		{
			const FHollowBreachNetState& State = It->GetIntegrity()->GetNetState();
			FString Zones;
			for (int32 Zone = 0; Zone < It->GetZoneCount(); ++Zone)
			{
				const TOptional<EHollowBreachPresentation> Shown = It->GetZonePresentation(Zone);
				Zones += FString::Printf(TEXT(" %d=%s%s"), Zone, Shown ? *HollowEnumToString(Shown.GetValue()) : TEXT("Standing"),
					It->GetLiveFracture(Zone) ? TEXT("(live)") : TEXT(""));
			}
			const FString Nav = It->HasAuthority() ? FString::Printf(TEXT("nav %s blocker %d"), *GetNameSafe(It->GetNavArea().Get()), It->IsNavBlockerActive())
				: FString(TEXT("nav: server only"));
			UE_LOG(LogHollowPhysics, Display, TEXT("Hollow.Breach.Dump: %s %s mask %02X stage %d serial %d zones:%s | %s"), *It->GetName(),
				*HollowEnumToString(State.State), State.ZoneMask, State.DamageStageQ, State.TransitionSerial, *Zones, *Nav);
		}
		if (const UHollowDestructionBudgetSubsystem* Budget = World->GetSubsystem<UHollowDestructionBudgetSubsystem>())
		{
			const FHollowDestructionBudget Limits = Budget->GetBudget();
			UE_LOG(LogHollowPhysics, Display, TEXT("Hollow.Breach.Dump: budget (effects quality %d) %d/%d live, %d/%d bodies, lifetime %.1f s, distance %.0f"),
				Scalability::GetQualityLevels().EffectsQuality, Budget->GetLiveBreaches(), Limits.MaxLiveBreaches, Budget->GetLiveBodies(),
				Limits.MaxActiveBodies, Limits.MaxDebrisLifetime, Limits.MaxSimDistance);
		}
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Breach.Dump"),
		TEXT("Prints this machine's bulkheads: state, zones and their presentation (Full/Snap), live debris, nav link and blocker, destruction budget."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
