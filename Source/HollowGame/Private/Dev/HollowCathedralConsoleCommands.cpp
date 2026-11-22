// Development console commands for the Reactor Cathedral's mechanics (S7-02, ADR-0039): what this machine sees of the
// Purification Altar and the electric arcs. Never shipped.
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "World/HollowArcHazard.h"
#include "World/HollowRitualAltar.h"

#if !UE_BUILD_SHIPPING

namespace HollowCathedralConsole
{
	/** Any machine: every altar it knows (replicated ones on a client): phase, time left, what lies on it. */
	void AltarDump(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		int32 Count = 0;
		for (TActorIterator<AHollowRitualAltar> It(World); It; ++It, ++Count)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Altar.Dump: %s"), *It->Describe());
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Altar.Dump: %d altar(s)"), Count);
	}

	/** Any machine: every arc it knows: power, phase on its clock, strikes (the host's), the players and their health. */
	void ArcDump(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		int32 Count = 0;
		for (TActorIterator<AHollowArcHazard> It(World); It; ++It, ++Count)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Arc.Dump: %s"), *It->Describe());
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Arc.Dump: %d arc(s)"), Count);
	}

	FAutoConsoleCommandWithWorld AltarDumpCommand(
		TEXT("Hollow.Altar.Dump"),
		TEXT("Prints every Purification Altar this machine knows: its phase, the ritual's time left, the core on it and what it counts as."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&AltarDump));

	FAutoConsoleCommandWithWorld ArcDumpCommand(
		TEXT("Hollow.Arc.Dump"),
		TEXT("Prints every electric arc this machine knows: power, phase, strikes, and each living player with health and whether inside."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ArcDump));
}

#endif // !UE_BUILD_SHIPPING
