#include "Visor/HollowVisorFrameWidget.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Settings/HollowGameUserSettings.h"
#include "Util/HollowEnumUtils.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVisorFrameWidget)

UHollowVisorFrameWidget::UHollowVisorFrameWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AlertParameter(TEXT("Alert"))
	, AlertColorParameter(TEXT("AlertColor"))
	, PulseParameter(TEXT("PulseHz"))
	, InterferenceParameter(TEXT("Interference"))
	, DamageParameter(TEXT("Damage"))
	, DamageColorParameter(TEXT("DamageColor"))
{
	SetVisibility(ESlateVisibility::HitTestInvisible);   // glass: seen, never clicked
}

void UHollowVisorFrameWidget::BindContext(UHollowVisorContext& InContext)
{
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		ReducedFlashesHandle = Settings->OnReducedFlashesChanged.AddUObject(this, &ThisClass::HandleReducedFlashesChanged);
	}
	Rebind();
}

void UHollowVisorFrameWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnSourcesChanged.Remove(SourcesHandle);
	if (UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get())
	{
		Settings->OnReducedFlashesChanged.Remove(ReducedFlashesHandle);
	}
	TierWatch.Unbind();
	HealthWatch.Unbind();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimer);
	}
	ShowDamage(0.f);
}

void UHollowVisorFrameWidget::HandleReducedFlashesChanged()
{
	Apply(GetTier());
}

void UHollowVisorFrameWidget::Rebind()
{
	UHollowAbilitySystemComponent* AbilitySystem = GetContext() ? GetContext()->GetAbilitySystem() : nullptr;
	TierWatch.Bind(AbilitySystem, [this](const EHollowResonanceTier Tier) { Apply(Tier); });   // calm glass without a body
	LastHealth = -1.f;   // the first value is where it stands, not a loss
	if (AbilitySystem)
	{
		HealthWatch.Bind(AbilitySystem, UHollowHealthSet::GetHealthAttribute(), UHollowHealthSet::GetMaxHealthAttribute(),
			[this](const float Value, const float Max) { HandleHealth(Value, Max); });
	}
	else
	{
		HealthWatch.Unbind();
	}
}

void UHollowVisorFrameWidget::HandleHealth(const float Value, const float Max)
{
	if (LastHealth >= 0.f && Value < LastHealth && Max > 0.f)
	{
		FlashDamage((LastHealth - Value) / Max);
	}
	LastHealth = Value;
}

void UHollowVisorFrameWidget::FlashDamage(const float Share)
{
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	const UWorld* World = GetWorld();
	if (!Style || !World || Share <= 0.f)
	{
		return;
	}
	const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	bDamageReduced = Settings && Settings->IsReducedFlashes();
	const float Strength = FMath::Min(Share / Style->DamageFullShare, 1.f);
	// A second hit while it fades starts it again, from at least where it is.
	DamagePeak = FMath::Max(DamageLevel, bDamageReduced ? Strength * Style->ReducedDamageLevel : Strength);
	DamageSeconds = bDamageReduced ? Style->ReducedDamageSeconds : Style->DamageFlashSeconds;
	DamageStart = World->GetTimeSeconds();
	if (DamageSeconds <= 0.f || DamagePeak <= 0.f)
	{
		return;
	}
	ShowDamage(DamagePeak);
	World->GetTimerManager().SetTimer(DamageTimer, FTimerDelegate::CreateUObject(this, &ThisClass::StepDamage), 1.f / Style->DamageFadeRateHz,
		/*bLoop=*/true);
}

void UHollowVisorFrameWidget::StepDamage()
{
	const UWorld* World = GetWorld();
	const float Progress = World && DamageSeconds > 0.f ? static_cast<float>((World->GetTimeSeconds() - DamageStart) / DamageSeconds) : 1.f;
	if (Progress >= 1.f)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(DamageTimer);
		}
		ShowDamage(0.f);
		return;
	}
	const float Left = 1.f - Progress;
	ShowDamage(DamagePeak * (bDamageReduced ? Left : Left * Left));   // reduced: a slow, even fade, no flash
}

void UHollowVisorFrameWidget::ShowDamage(const float Level)
{
	DamageLevel = Level;
	if (EnsureMaterial())
	{
		GlassMaterial->SetScalarParameterValue(DamageParameter, Level);
		if (const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr)
		{
			GlassMaterial->SetVectorParameterValue(DamageColorParameter, Style->DamageColor);
		}
	}
}

bool UHollowVisorFrameWidget::IsDamageFading() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(DamageTimer);
}

bool UHollowVisorFrameWidget::EnsureMaterial()
{
	if (!GlassMaterial && Glass)
	{
		GlassMaterial = Glass->GetDynamicMaterial();   // null if the brush holds no material
	}
	return GlassMaterial != nullptr;
}

void UHollowVisorFrameWidget::Apply(const EHollowResonanceTier Tier)
{
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	static const FHollowVisorTierLook Calm;
	const FHollowVisorTierLook& Look = Style ? Style->GetTierLook(Tier) : Calm;
	const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	Alert = Look.Alert;
	PulseHz = Settings && Settings->IsReducedFlashes() ? 0.f : FMath::Clamp(Look.PulseHz, 0.f, UHollowVisorStyle::MaxPulseHz);   // ADR-0029: steady
	if (EnsureMaterial())
	{
		GlassMaterial->SetScalarParameterValue(AlertParameter, Look.Alert);
		GlassMaterial->SetVectorParameterValue(AlertColorParameter, Look.Color);
		GlassMaterial->SetScalarParameterValue(PulseParameter, PulseHz);
		GlassMaterial->SetScalarParameterValue(InterferenceParameter, Look.Interference);
	}
}

FString UHollowVisorFrameWidget::Describe() const
{
	return FString::Printf(TEXT("glass %s, alert %.2f, pulse %.1f Hz, damage %.2f%s"), *HollowEnumToString(GetTier()), Alert, PulseHz, DamageLevel,
		GlassMaterial ? TEXT("") : TEXT(" (no material)"));
}
