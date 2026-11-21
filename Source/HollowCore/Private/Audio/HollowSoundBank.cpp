#include "Audio/HollowSoundBank.h"

#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSettings.h"
#include "Sound/SoundBase.h"
#include "Tags/HollowGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSoundBank)

#define LOCTEXT_NAMESPACE "HollowSoundBank"

FGameplayTag FHollowSoundEntry::GetNoiseTag() const
{
	return SoundTag.MatchesTag(HollowTags::Noise) ? SoundTag : Noise;
}

const UHollowSoundBank* UHollowSoundBank::Get()
{
	return GetDefault<UHollowAudioSettings>()->SoundBank.LoadSynchronous();
}

const FHollowSoundEntry* UHollowSoundBank::Find(const FGameplayTag& SoundTag) const
{
	return Entries.FindByPredicate([&SoundTag](const FHollowSoundEntry& Entry) { return Entry.SoundTag == SoundTag; });
}

#if WITH_EDITOR
EDataValidationResult UHollowSoundBank::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UHollowNoiseConfig* Noise = GetDefault<UHollowNoiseSettings>()->Config.LoadSynchronous();
	if (!Noise)
	{
		Context.AddError(LOCTEXT("Noise", "No DA_NoiseConfig to agree with (UHollowNoiseSettings::Config)."));
		return EDataValidationResult::Invalid;
	}
	for (const FString& Problem : HollowSoundRules::Check(Entries, *Noise))
	{
		Context.AddError(FText::FromString(Problem));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

float HollowSoundRules::AudibleRange(const FHollowNoiseRule& Rule, const float ReferenceHearingRange)
{
	return GetDefault<UHollowNoiseConfig>()->RangeFor(Rule, ReferenceHearingRange);   // the AI's formula, the reference listener
}

float HollowSoundRules::AudibleRange(const FHollowSoundEntry& Entry, const UHollowNoiseConfig& Noise, const float ReferenceHearingRange)
{
	const FGameplayTag NoiseTag = Entry.GetNoiseTag();
	if (!NoiseTag.IsValid())
	{
		return Entry.AudibleRange;
	}
	const FHollowNoiseRule* Rule = Noise.FindRule(NoiseTag);
	return Rule ? AudibleRange(*Rule, ReferenceHearingRange) : 0.f;
}

TArray<FString> HollowSoundRules::Check(const TConstArrayView<FHollowSoundEntry> Entries, const UHollowNoiseConfig& Noise)
{
	TArray<FString> Problems;
	for (const FHollowNoiseRule& Rule : Noise.Rules)
	{
		if (Rule.NoiseTag.MatchesTag(HollowTags::Noise_Voice))
		{
			continue;   // players hear a voice through its VOIP talker, not the bank (S4-07)
		}
		if (!Entries.ContainsByPredicate([&Rule](const FHollowSoundEntry& Entry) { return Entry.GetNoiseTag() == Rule.NoiseTag; }))
		{
			Problems.Add(FString::Printf(TEXT("%s: the AI hears it, players cannot (no sound)"), *Rule.NoiseTag.ToString()));
		}
	}
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FHollowSoundEntry& Entry = Entries[Index];
		const FString Name = Entry.SoundTag.ToString();
		if (!Entry.SoundTag.MatchesTag(HollowTags::Noise) && !Entry.SoundTag.MatchesTag(HollowTags::Sound))
		{
			Problems.Add(FString::Printf(TEXT("%s: neither Hollow.Noise.* nor Hollow.Sound.*"), *Name));
		}
		if (Entry.Sound.IsNull())
		{
			Problems.Add(FString::Printf(TEXT("%s: no sound"), *Name));
		}
		for (int32 Other = Index + 1; Other < Entries.Num(); ++Other)
		{
			if (Entries[Other].SoundTag == Entry.SoundTag)
			{
				Problems.Add(FString::Printf(TEXT("%s: listed twice"), *Name));
			}
		}
		const FGameplayTag NoiseTag = Entry.GetNoiseTag();
		if (NoiseTag.IsValid() && !Noise.FindRule(NoiseTag))
		{
			Problems.Add(FString::Printf(TEXT("%s: linked to %s, which has no noise rule"), *Name, *NoiseTag.ToString()));
		}
		if (!NoiseTag.IsValid() && Entry.AudibleRange <= 0.f)
		{
			Problems.Add(FString::Printf(TEXT("%s: a players-only sound needs its AudibleRange"), *Name));
		}
		if (NoiseTag.IsValid() && Entry.AudibleRange > 0.f)
		{
			Problems.Add(FString::Printf(TEXT("%s: carries as far as its noise; AudibleRange would be a second number"), *Name));
		}
	}
	return Problems;
}

#undef LOCTEXT_NAMESPACE
