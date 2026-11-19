#include "Framework/HollowPlayerState.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowSurvivalSet.h"
#include "Attributes/HollowToolSet.h"
#include "Session/HollowNetworkSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPlayerState)

AHollowPlayerState::AHollowPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UHollowAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthSet = CreateDefaultSubobject<UHollowHealthSet>(TEXT("HealthSet"));
	SurvivalSet = CreateDefaultSubobject<UHollowSurvivalSet>(TEXT("SurvivalSet"));
	ResonanceSet = CreateDefaultSubobject<UHollowResonanceSet>(TEXT("ResonanceSet"));
	ToolSet = CreateDefaultSubobject<UHollowToolSet>(TEXT("ToolSet"));

	// The engine default for player states is far too low for an ability system owner.
	const float NetUpdateFrequency = GetDefault<UHollowNetworkSettings>()->PlayerStateNetUpdateFrequency;
	if (NetUpdateFrequency > 0.f)
	{
		SetNetUpdateFrequency(NetUpdateFrequency);
	}
}

UAbilitySystemComponent* AHollowPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
