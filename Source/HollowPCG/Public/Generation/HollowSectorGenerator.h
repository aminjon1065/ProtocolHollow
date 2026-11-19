#pragma once

#include "Generation/HollowSectorGenTypes.h"

/**
 * Deterministic sector generator v0 (Docs/Architecture/02 sec. 2.6, ADR-0003). Pure C++ on an integer grid: the same
 * seed, rules and template pool give a bit-identical layout on every platform. Runs on the server only.
 *
 * One attempt: critical path Spawn -> Path rooms -> Extraction; one side branch per core vault; loops (adjacent free
 * connectors, then bridges of up to MaxBridgeRooms rooms) until MinLoops; optional side branches; connection kinds.
 * A failed attempt is retried with HollowRandom::DeriveSeed(Seed, Attempt), up to MaxAttempts.
 */
class HOLLOWPCG_API FHollowSectorGenerator
{
public:
	static EHollowGenResult Generate(const FHollowSectorGenInput& Input, FHollowSectorLayout& OutLayout, FHollowSectorGenReport* OutReport = nullptr);

	/** Problems of the rules alone (also used by Data Validation of biome definitions). */
	static TArray<FString> ValidateRules(const FHollowSectorGenRules& Rules);

	/** Problems of rules plus template pool; empty means Generate can run. */
	static TArray<FString> ValidateInput(const FHollowSectorGenInput& Input);

	/** CRC32 of the canonical layout: template ids, cells, sizes, rotations, roles, connections. Endian- and TCHAR-independent. */
	static uint32 ComputeLayoutHash(const FHollowSectorLayout& Layout);

	/** ASCII map (north up) plus legend, for logs and the Hollow.PCG.Generate command. */
	static TArray<FString> RenderAscii(const FHollowSectorLayout& Layout);
};
