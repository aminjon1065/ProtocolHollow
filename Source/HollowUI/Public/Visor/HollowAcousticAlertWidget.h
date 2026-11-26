#pragma once

#include "GameplayTagContainer.h"
#include "Visor/HollowVisorElement.h"

#include "HollowAcousticAlertWidget.generated.h"

class AHollowPlayerController;
class UTextBlock;

/**
 * POLISH-01 (ADR-0053): "ACOUSTIC ALERT" at the top of the visor when an enemy woke to this player's noise (the
 * server's AHollowPlayerController::Client_AcousticAlert, at most once per cooldown), steady for DA_VisorStyle's
 * AcousticAlertSeconds (a timer), so the player knows why the hunt began. Event driven, never ticks.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowAcousticAlertWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	bool IsShown() const { return bShown; }
	FText GetAlertText() const;
	virtual FString Describe() const override;

	/** An enemy answered this player's NoiseTag (as its controller tells it; tests). */
	void ShowAlert(const FGameplayTag& NoiseTag);

	/** Hides it now (the timer's end; tests). */
	void Hide();

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AlertText;

private:
	TWeakObjectPtr<AHollowPlayerController> Controller;
	FDelegateHandle AlertHandle;
	FTimerHandle HideTimer;
	FGameplayTag LastNoise;
	bool bShown = false;
};
