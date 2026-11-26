#include "World/HollowFxPlayerComponent.h"

#include "Engine/StreamableManager.h"
#include "Particles/ParticleSystem.h"   // UFXSystemAsset
#include "Settings/HollowGameUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFxPlayerComponent)

UHollowFxPlayerComponent::UHollowFxPlayerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UHollowFxPlayerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		FlashesHandle = Settings->OnReducedFlashesChanged.AddUObject(this, &ThisClass::Restart);
	}
	FxHandle = HollowFx::Preload({&Effect}, FSimpleDelegate::CreateWeakLambda(this, [this] { Restart(); }));
}

void UHollowFxPlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		Settings->OnReducedFlashesChanged.Remove(FlashesHandle);
	}
	FlashesHandle.Reset();
	HollowFx::Stop(Playing);
	Playing = nullptr;
	FxHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void UHollowFxPlayerComponent::SetEffect(const FHollowFxSpec& InEffect)
{
	Effect = InEffect;
	if (!HasBegunPlay())
	{
		return;   // BeginPlay loads and plays it
	}
	// Already in memory: at once (a load completes on a later frame); the handle keeps both variants for a switch.
	const bool bLoaded = HollowFx::Chosen(Effect).Get() != nullptr;
	FxHandle = HollowFx::Preload({&Effect}, bLoaded ? FSimpleDelegate() : FSimpleDelegate::CreateWeakLambda(this, [this] { Restart(); }));
	if (bLoaded)
	{
		Restart();
	}
}

void UHollowFxPlayerComponent::Restart()
{
	HollowFx::Stop(Playing);
	Playing = HollowFx::PlayAttached(Effect, this);
}
