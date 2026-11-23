#include "Tools/HollowLanternCue.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/SpotLightComponent.h"
#include "Data/HollowLanternDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HollowGameLog.h"
#include "Settings/HollowGameUserSettings.h"
#include "TimerManager.h"
#include "Tools/HollowToolAim.h"
#include "Tools/HollowToolMountComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLanternCue)

AHollowLanternCue::AHollowLanternCue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bAutoDestroyOnRemove = true;

	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Light->SetCastShadows(false);
	Light->SetVisibility(false);
	SetRootComponent(Light);
}

bool AHollowLanternCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	Wielder = Cast<APawn>(MyTarget);
	Lantern = Cast<UHollowLanternDefinition>(Parameters.SourceObject.Get());
	if (!Wielder.IsValid() || !Lantern.IsValid() || UpdateRateHz <= 0.f)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("%s: the lantern light needs a pawn, a lantern definition and UpdateRateHz."), *GetName());
		return false;
	}
	const UHollowLanternDefinition* Definition = Lantern.Get();
	Light->SetOuterConeAngle(Definition->ConeHalfAngleDegrees);
	Light->SetInnerConeAngle(Definition->ConeHalfAngleDegrees * InnerConeFraction);
	Light->SetAttenuationRadius(Definition->Range);
	Light->SetLightColor(Definition->BeamColor);
	if (UHollowToolMountComponent* Mount = UHollowToolMountComponent::Get(Wielder.Get()))
	{
		Mount->ShowTool(Definition);   // the lantern in hand on this machine too (cosmetic; the cue is what replicates)
		Mount->SetFiring(true);        // Art-01: its flame burns bright while it runs
	}
	BaseIntensity = GetClass()->GetDefaultObject<AHollowLanternCue>()->Light->Intensity;
	LitAt = GetWorld()->GetTimeSeconds();
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
	{
		Sound = Audio->PlayAttached(Definition->NoiseTag, Wielder->GetRootComponent());
	}
	UpdateLight();
	Light->SetVisibility(true);
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateLight, 1.f / UpdateRateHz, /*bLoop=*/true);
	UE_LOG(LogHollowGame, Log, TEXT("Lantern light on: %s (%s, %s)"), *Wielder->GetName(), *Definition->ToolTag.ToString(),
		Wielder->IsLocallyControlled() ? TEXT("own") : TEXT("remote"));
	return true;
}

bool AHollowLanternCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	Light->SetVisibility(false);
	if (BaseIntensity > 0.f)
	{
		Light->SetIntensity(BaseIntensity);
	}
	if (UHollowToolMountComponent* Mount = UHollowToolMountComponent::Get(Wielder.Get()))
	{
		Mount->SetFiring(false);
	}
	if (Sound)
	{
		Sound->Stop();
		Sound = nullptr;
	}
	return Super::OnRemove_Implementation(MyTarget, Parameters);
}

void AHollowLanternCue::UpdateLight()
{
	const APawn* Pawn = Wielder.Get();
	if (!Pawn)
	{
		return;
	}
	FHollowToolAimTrace Aim;
	Aim.Eyes = Pawn->GetPawnViewLocation();
	Aim.Aim = Pawn->GetBaseAimRotation();
	SetActorLocationAndRotation(HollowToolAim::BeamStart(*Pawn, Aim, MuzzleOffset), Aim.Aim);
	Light->SetIntensity(GetIntensity());
}

float AHollowLanternCue::GetIntensity() const
{
	const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	const bool bReduced = Settings && Settings->IsReducedFlashes();
	const double Since = GetWorld() ? GetWorld()->GetTimeSeconds() - LitAt : FlashSeconds;
	const float Settle = FlashSeconds > 0.f ? FMath::Clamp(static_cast<float>(Since) / FlashSeconds, 0.f, 1.f) : 1.f;
	return BaseIntensity * (bReduced ? 1.f : FMath::Lerp(FlashIntensityScale, 1.f, Settle));
}
