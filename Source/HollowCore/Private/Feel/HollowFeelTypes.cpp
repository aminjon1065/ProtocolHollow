#include "Feel/HollowFeelTypes.h"

#include "Tags/HollowGameplayTags.h"
#include "Templates/TypeHash.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFeelTypes)

namespace HollowFeelTypesPrivate
{
	/** Each axis swings at its own rate around the shake's frequency (so the axes never move in step). */
	constexpr float AxisRateBase = 0.85f;
	constexpr float AxisRateStep = 0.07f;
	/** A second, faster sine on top of the first: the shake's grain. */
	constexpr float OvertoneRate = 2.3f;
	constexpr float BaseWeight = 0.7f;
	constexpr float OvertoneWeight = 0.3f;
	constexpr uint32 PhaseResolution = 1u << 16;
	constexpr int32 Axes = 6;

	float Phase(const uint32 Seed, const uint32 Salt)
	{
		return static_cast<float>(HashCombineFast(Seed, Salt) % PhaseResolution) / PhaseResolution * UE_TWO_PI;
	}

	float Wave(const FHollowShakeParams& Shake, const uint32 Seed, const int32 Axis, const float Time)
	{
		const float Rate = Shake.Frequency * (AxisRateBase + AxisRateStep * Axis) * UE_TWO_PI;
		return BaseWeight * FMath::Sin(Rate * Time + Phase(Seed, 2 * Axis)) + OvertoneWeight * FMath::Sin(OvertoneRate * Rate * Time + Phase(Seed, 2 * Axis + 1));
	}
}

bool FHollowFeelEntry::IsPersonal() const
{
	return Trigger.MatchesTag(HollowTags::Feel);
}

float HollowFeelRules::ScaleAt(const FHollowFeelEntry& Entry, const float Distance)
{
	if (Entry.IsPersonal())
	{
		return 1.f;
	}
	if (Distance <= Entry.InnerRadius)
	{
		return Entry.OuterRadius > 0.f ? 1.f : 0.f;
	}
	if (Distance >= Entry.OuterRadius)
	{
		return 0.f;
	}
	return 1.f - (Distance - Entry.InnerRadius) / FMath::Max(Entry.OuterRadius - Entry.InnerRadius, UE_KINDA_SMALL_NUMBER);
}

float HollowFeelRules::Envelope(const FHollowShakeParams& Shake, const float Time)
{
	if (Time < 0.f || Time >= Shake.Duration || Shake.Duration <= 0.f)
	{
		return 0.f;
	}
	const float Rise = Shake.BlendIn > 0.f ? FMath::Min(Time / Shake.BlendIn, 1.f) : 1.f;
	const float Fall = 1.f - Time / Shake.Duration;
	return Rise * Fall * Fall;
}

void HollowFeelRules::Sample(const FHollowShakeParams& Shake, const uint32 Seed, const float Time, FRotator& OutRotation, FVector& OutLocation)
{
	using namespace HollowFeelTypesPrivate;
	const float Weight = Envelope(Shake, Time);
	if (Weight <= 0.f || !Shake.IsValid())
	{
		OutRotation = FRotator::ZeroRotator;
		OutLocation = FVector::ZeroVector;
		return;
	}
	float Values[Axes];
	for (int32 Axis = 0; Axis < Axes; ++Axis)
	{
		Values[Axis] = Weight * Wave(Shake, Seed, Axis, Time);
	}
	OutRotation = FRotator(Shake.Rotation.Pitch * Values[0], Shake.Rotation.Yaw * Values[1], Shake.Rotation.Roll * Values[2]);
	OutLocation = FVector(Shake.Location.X * Values[3], Shake.Location.Y * Values[4], Shake.Location.Z * Values[5]);
}

float HollowFeelRules::KickLeft(const FHollowViewKick& Kick, const float Elapsed)
{
	if (Elapsed < 0.f || Kick.Recover <= 0.f || Elapsed >= Kick.Recover)
	{
		return 0.f;
	}
	const float Progress = Elapsed / Kick.Recover;
	return 1.f - Progress * Progress * (3.f - 2.f * Progress);   // smoothstep back to rest
}

TArray<FString> HollowFeelRules::Check(const TConstArrayView<FHollowFeelEntry> Entries)
{
	TArray<FString> Problems;
	TSet<FGameplayTag> Seen;
	for (const FHollowFeelEntry& Entry : Entries)
	{
		const FString Name = Entry.Trigger.ToString();
		if (!Entry.Trigger.IsValid())
		{
			Problems.Add(TEXT("an entry without a trigger"));
			continue;
		}
		if (Seen.Contains(Entry.Trigger))
		{
			Problems.Add(FString::Printf(TEXT("%s: twice"), *Name));
		}
		Seen.Add(Entry.Trigger);
		if (!Entry.Shake.IsValid() && !Entry.Rumble.IsValid() && !Entry.Kick.IsValid())
		{
			Problems.Add(FString::Printf(TEXT("%s: does nothing"), *Name));
		}
		if (!Entry.IsPersonal())
		{
			if (Entry.OuterRadius <= 0.f || Entry.OuterRadius < Entry.InnerRadius)
			{
				Problems.Add(FString::Printf(TEXT("%s: a sound's feel needs an outer radius past its inner one"), *Name));
			}
			if (Entry.Kick.IsValid())
			{
				Problems.Add(FString::Printf(TEXT("%s: a view kick is one player's own (a Hollow.Feel.* trigger)"), *Name));
			}
		}
	}
	return Problems;
}
