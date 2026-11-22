#pragma once

#include "Visor/HollowVisorElement.h"

#include "HollowLampIndicatorWidget.generated.h"

class AHollowExoFrameCharacter;
class UTextBlock;

/**
 * Visor telemetry of the helmet lamp (S4-08): "[F] LAMP ON" with the lamp's key on the current device. Follows the
 * player's ExoFrame (its lamp switches on the owner at once, predicted); hidden without a body.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowLampIndicatorWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	FText GetLampText() const;
	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LampText;

private:
	void Rebind();
	void HandleLamp(bool bOn);
	void Refresh();

	TWeakObjectPtr<AHollowExoFrameCharacter> Character;
	FDelegateHandle SourcesHandle;
	FDelegateHandle InputTypeHandle;
	FDelegateHandle KeysHandle;
	FDelegateHandle ClassHandle;
	FDelegateHandle LampHandle;
};
