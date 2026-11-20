#include "Noise/HollowNoiseConfig.h"

#include "Sector/HollowSectorTopology.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowNoiseConfig)

#define LOCTEXT_NAMESPACE "HollowNoiseConfig"

const FHollowNoiseRule* UHollowNoiseConfig::FindRule(const FGameplayTag& NoiseTag) const
{
	return NoiseTag.IsValid() ? Rules.FindByPredicate([&NoiseTag](const FHollowNoiseRule& Rule) { return Rule.NoiseTag == NoiseTag; }) : nullptr;
}

float UHollowNoiseConfig::RangeFor(const FHollowNoiseRule& Rule, const float HearingRange, const float LoudnessScale) const
{
	const float Range = Rule.Loudness * LoudnessScale * HearingRange;
	return Rule.MaxRange > 0.f ? FMath::Min(Range, Rule.MaxRange) : Range;
}

bool UHollowNoiseConfig::Reaches(const FHollowNoiseRule& Rule, const float HearingRange, const float LoudnessScale, const float Distance,
	const bool bFromAnotherRoom) const
{
	return Distance <= RangeFor(Rule, HearingRange, LoudnessScale) * (bFromAnotherRoom ? InterRoomRange : 1.f);
}

bool UHollowNoiseConfig::ReachesThroughWalls(const FHollowSectorTopology& Topology, const FHollowNoiseRule& Rule, const float HearingRange,
	const float LoudnessScale, const FVector& Source, const FVector& Listener) const
{
	const int32 From = Topology.RoomAt(Source);
	const int32 To = Topology.RoomAt(Listener);
	if (From == INDEX_NONE || To == INDEX_NONE || From == To)
	{
		return true;
	}
	return Reaches(Rule, HearingRange, LoudnessScale, FVector::Dist(Source, Listener), /*bFromAnotherRoom=*/true);
}

float UHollowNoiseConfig::FootstepLoudnessScale(const float Speed) const
{
	return Speed < FootstepMinSpeed || FootstepReferenceSpeed <= 0.f ? 0.f : Speed / FootstepReferenceSpeed;
}

#if WITH_EDITOR
EDataValidationResult UHollowNoiseConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		const FGameplayTag& Tag = Rules[Index].NoiseTag;
		const bool bDuplicate = Rules.IndexOfByPredicate([&Tag](const FHollowNoiseRule& Rule) { return Rule.NoiseTag == Tag; }) != Index;
		if (!Tag.IsValid() || bDuplicate)
		{
			Context.AddError(FText::Format(LOCTEXT("BadRule", "Noise rule {0} needs a tag of its own."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
