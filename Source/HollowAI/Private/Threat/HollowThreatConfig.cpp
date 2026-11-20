#include "Threat/HollowThreatConfig.h"

#include "HollowAISettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowThreatConfig)

const UHollowThreatConfig* UHollowThreatConfig::Get()
{
	return GetDefault<UHollowAISettings>()->ThreatConfig.LoadSynchronous();
}
