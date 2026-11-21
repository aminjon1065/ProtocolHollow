#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"

#include "HollowResonanceTypes.generated.h"

/** How deep the squad is in the Aether (ADR-0020): picks the strength of distortions and of the AI's senses. */
UENUM(BlueprintType)
enum class EHollowResonanceTier : uint8
{
	Low,
	Medium,
	High,
	Critical
};
ENUM_RANGE_BY_FIRST_AND_LAST(EHollowResonanceTier, EHollowResonanceTier::Low, EHollowResonanceTier::Critical);

/** What the server replicates of the squad's Resonance: 2 bytes, at most a few times a second. */
USTRUCT()
struct HOLLOWGAS_API FHollowSquadResonanceNetState
{
	GENERATED_BODY()

	/** Squad Resonance 0..1 quantized to 0..255 (HollowResonanceMath::Quantize). */
	UPROPERTY()
	uint8 Level = 0;

	UPROPERTY()
	EHollowResonanceTier Tier = EHollowResonanceTier::Low;

	bool operator==(const FHollowSquadResonanceNetState&) const = default;
};
