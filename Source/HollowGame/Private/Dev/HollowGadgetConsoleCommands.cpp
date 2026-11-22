// Development console commands for gadgets (S6-03, ADR-0036): what this machine sees of the mines and wards. Never shipped.
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gadgets/HollowOccultWard.h"
#include "Gadgets/HollowPulseMine.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"

#if !UE_BUILD_SHIPPING

namespace HollowGadgetConsole
{
	/** Any machine: every pulse mine and ritual ward it knows (replicated ones on a client), with what is left of a ward. */
	void Dump(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		int32 Count = 0;
		for (TActorIterator<AHollowPulseMine> It(World); It; ++It, ++Count)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Gadget.Dump: mine %s of %s at %s, armed %d, gone off %d"), *It->GetName(), *GetNameSafe(It->GetInstigator()),
				*It->GetActorLocation().ToCompactString(), It->IsArmed(), It->HasDetonated());
		}
		for (TActorIterator<AHollowOccultWard> It(World); It; ++It, ++Count)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Gadget.Dump: ward %s"), *It->Describe());
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Gadget.Dump: %d gadget(s)"), Count);
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Gadget.Dump"),
		TEXT("Prints every pulse mine and ritual ward this machine knows: where, whose, and how long a ward still burns."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}

#endif // !UE_BUILD_SHIPPING
