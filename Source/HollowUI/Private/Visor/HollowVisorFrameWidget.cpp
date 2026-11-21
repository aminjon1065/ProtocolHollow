#include "Visor/HollowVisorFrameWidget.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
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
{
	SetVisibility(ESlateVisibility::HitTestInvisible);   // glass: seen, never clicked
}

void UHollowVisorFrameWidget::BindContext(UHollowVisorContext& InContext)
{
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	Rebind();
}

void UHollowVisorFrameWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnSourcesChanged.Remove(SourcesHandle);
	TierWatch.Unbind();
}

void UHollowVisorFrameWidget::Rebind()
{
	UHollowAbilitySystemComponent* AbilitySystem = GetContext() ? GetContext()->GetAbilitySystem() : nullptr;
	TierWatch.Bind(AbilitySystem, [this](const EHollowResonanceTier Tier) { Apply(Tier); });   // calm glass without a body
}

void UHollowVisorFrameWidget::Apply(const EHollowResonanceTier Tier)
{
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	static const FHollowVisorTierLook Calm;
	const FHollowVisorTierLook& Look = Style ? Style->GetTierLook(Tier) : Calm;
	Alert = Look.Alert;
	if (!GlassMaterial && Glass)
	{
		GlassMaterial = Glass->GetDynamicMaterial();   // null if the brush holds no material
	}
	if (GlassMaterial)
	{
		GlassMaterial->SetScalarParameterValue(AlertParameter, Look.Alert);
		GlassMaterial->SetVectorParameterValue(AlertColorParameter, Look.Color);
		GlassMaterial->SetScalarParameterValue(PulseParameter, FMath::Clamp(Look.PulseHz, 0.f, UHollowVisorStyle::MaxPulseHz));
		GlassMaterial->SetScalarParameterValue(InterferenceParameter, Look.Interference);
	}
}

FString UHollowVisorFrameWidget::Describe() const
{
	return FString::Printf(TEXT("glass %s, alert %.2f%s"), *HollowEnumToString(GetTier()), Alert, GlassMaterial ? TEXT("") : TEXT(" (no material)"));
}
