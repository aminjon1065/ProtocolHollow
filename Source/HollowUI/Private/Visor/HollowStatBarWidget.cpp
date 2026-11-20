#include "Visor/HollowStatBarWidget.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "ViewModels/HollowVisorContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowStatBarWidget)

void UHollowStatBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (LabelText)
	{
		LabelText->SetText(Label);
	}
	ApplyLook();
}

void UHollowStatBarWidget::BindContext(UHollowVisorContext& InContext)
{
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	Rebind();
}

void UHollowStatBarWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnSourcesChanged.Remove(SourcesHandle);
	Watch.Unbind();
	if (UAbilitySystemComponent* Source = AlertSource.Get())
	{
		Source->RegisterGameplayTagEvent(AlertTag, EGameplayTagEventType::NewOrRemoved).Remove(AlertHandle);
	}
	AlertSource.Reset();
}

void UHollowStatBarWidget::Rebind()
{
	UHollowAbilitySystemComponent* AbilitySystem = GetContext() ? GetContext()->GetAbilitySystem() : nullptr;
	Watch.Bind(AbilitySystem, Attribute, MaxAttribute, [this](const float Value, const float Max) { HandleChanged(Value, Max); });
	if (!AbilitySystem)
	{
		HandleChanged(0.f, 0.f);
	}

	if (UAbilitySystemComponent* Old = AlertSource.Get())
	{
		Old->RegisterGameplayTagEvent(AlertTag, EGameplayTagEventType::NewOrRemoved).Remove(AlertHandle);
	}
	AlertSource = AbilitySystem;
	bAlert = false;
	if (AbilitySystem && AlertTag.IsValid())
	{
		AlertHandle = AbilitySystem->RegisterGameplayTagEvent(AlertTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleAlertTag);
		bAlert = AbilitySystem->HasMatchingGameplayTag(AlertTag);
	}
	ApplyLook();
}

void UHollowStatBarWidget::HandleChanged(const float Value, const float Max)
{
	if (Bar)
	{
		Bar->SetPercent(FHollowAttributeWatch::Ratio(Value, Max));
	}
	if (ValueText)
	{
		ValueText->SetText(FText::AsNumber(FMath::RoundToInt(Value)));
	}
}

void UHollowStatBarWidget::HandleAlertTag(const FGameplayTag Tag, const int32 Count)
{
	bAlert = Count > 0;
	ApplyLook();
}

void UHollowStatBarWidget::ApplyLook()
{
	if (Bar)
	{
		Bar->SetFillColorAndOpacity(bAlert ? AlertColor : FillColor);
	}
}
