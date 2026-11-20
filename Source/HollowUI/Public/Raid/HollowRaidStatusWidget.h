#pragma once

#include "CommonUserWidget.h"
#include "Raid/HollowRaidStateTypes.h"

#include "HollowRaidStatusWidget.generated.h"

class UHollowRaidStateComponent;
class UTextBlock;

/**
 * One visor line about the raid (ADR-0016): deploying, the shuttle's countdown with who and what is aboard, raid over.
 * Follows the replicated raid state; a timer refreshes the countdown only while a phase is timed (no Tick).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowRaidStatusWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	explicit UHollowRaidStatusWidget(const FObjectInitializer& ObjectInitializer);

	FText GetStatus() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

private:
	void TryBind();
	void HandleStateChanged(const FHollowRaidNetState& OldState, const FHollowRaidNetState& NewState);
	void Refresh();

	TWeakObjectPtr<UHollowRaidStateComponent> Raid;
	FDelegateHandle StateHandle;
	FTimerHandle RefreshTimer;
	FTimerHandle BindTimer;
};
