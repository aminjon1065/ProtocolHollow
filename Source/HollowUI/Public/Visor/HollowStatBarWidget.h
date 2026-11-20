#pragma once

#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "ViewModels/HollowAttributeWatch.h"
#include "Visor/HollowVisorElement.h"

#include "HollowStatBarWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;

/**
 * Visor bar of one attribute against its maximum (health, tool fuel, tool heat). Which attribute is instance data
 * set in W_Visor; the bar follows GAS change delegates, so clients update when replicated values arrive.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowStatBarWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	const FText& GetLabel() const { return Label; }
	const FGameplayAttribute& GetAttribute() const { return Attribute; }
	float GetValue() const { return Watch.GetValue(); }
	float GetMax() const { return Watch.GetMax(); }
	bool IsAlert() const { return bAlert; }

protected:
	virtual void NativePreConstruct() override;
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(EditAnywhere, Category = "Visor")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, Category = "Visor")
	FGameplayAttribute MaxAttribute;

	UPROPERTY(EditAnywhere, Category = "Visor")
	FText Label;

	/** While the owner has this tag the bar uses AlertColor (e.g. Hollow.State.Tool.Overheated). */
	UPROPERTY(EditAnywhere, Category = "Visor")
	FGameplayTag AlertTag;

	UPROPERTY(EditAnywhere, Category = "Visor")
	FLinearColor FillColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Visor")
	FLinearColor AlertColor = FLinearColor::Red;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

private:
	void Rebind();
	void HandleChanged(float Value, float Max);
	void HandleAlertTag(FGameplayTag Tag, int32 Count);
	void ApplyLook();

	FHollowAttributeWatch Watch;
	TWeakObjectPtr<UAbilitySystemComponent> AlertSource;
	FDelegateHandle SourcesHandle;
	FDelegateHandle AlertHandle;
	bool bAlert = false;
};
