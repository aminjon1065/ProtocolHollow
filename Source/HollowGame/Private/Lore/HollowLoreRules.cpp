#include "Lore/HollowLoreRules.h"

namespace HollowLoreRules
{
	FPrimaryAssetId PickEntry(TConstArrayView<FHollowLoreCandidate> Candidates, TConstArrayView<FPrimaryAssetId> Taken,
		TConstArrayView<TArray<FPrimaryAssetId>> Squad, const int32 Seed)
	{
		const FHollowLoreCandidate* Best = nullptr;
		int32 BestKnown = MAX_int32;
		uint32 BestKey = MAX_uint32;
		for (const FHollowLoreCandidate& Candidate : Candidates)
		{
			if (!Candidate.Id.IsValid() || Taken.Contains(Candidate.Id))
			{
				continue;
			}
			int32 Known = 0;
			for (const TArray<FPrimaryAssetId>& Player : Squad)
			{
				Known += Player.Contains(Candidate.Id) ? 1 : 0;
			}
			const uint32 Key = HashCombine(GetTypeHash(Seed), GetTypeHash(Candidate.Number));
			if (!Best || Known < BestKnown || (Known == BestKnown && (Key < BestKey || (Key == BestKey && Candidate.Number < Best->Number))))
			{
				Best = &Candidate;
				BestKnown = Known;
				BestKey = Key;
			}
		}
		return Best ? Best->Id : FPrimaryAssetId();
	}

	int32 CountKnown(TConstArrayView<FPrimaryAssetId> All, TConstArrayView<FPrimaryAssetId> Known)
	{
		int32 Count = 0;
		for (const FPrimaryAssetId& Id : All)
		{
			Count += Known.Contains(Id) ? 1 : 0;
		}
		return Count;
	}
}
