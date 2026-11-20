#include "Settings/HollowGraphicsSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGraphicsSettings)

const FHollowGraphicsPresetSpec* UHollowGraphicsSettings::FindPreset(const EHollowGraphicsPreset Preset) const
{
	return Presets.FindByPredicate([Preset](const FHollowGraphicsPresetSpec& Spec) { return Spec.Preset == Preset; });
}

EHollowGraphicsPreset UHollowGraphicsSettings::PresetForBenchmark(const float GpuScore) const
{
	return GpuScore < LowGpuBenchmarkScore ? EHollowGraphicsPreset::Low : EHollowGraphicsPreset::High;
}
