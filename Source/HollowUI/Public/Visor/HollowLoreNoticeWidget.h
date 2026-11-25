#pragma once

#include "Visor/HollowVisorElement.h"

#include "HollowLoreNoticeWidget.generated.h"

class UHollowRaidStateComponent;
class UTextBlock;
struct FPrimaryAssetId;

/**
 * The visor's notice of a downloaded crew log (NAR-01b, ADR-0050), under the objective: "CREW LOG 3/12", the log's
 * title, and what it brings ("+75 cr on extraction", or "already in your archive"). It follows the raid's found logs
 * (UHollowRaidStateComponent::OnLoreFound, every machine) and hides again after DA_VisorStyle's LoreNoticeSeconds
 * (a timer). Event driven, never ticks.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowLoreNoticeWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	bool IsShown() const { return bShown; }
	FText GetTitleText() const;
	FText GetEntryText() const;
	FText GetDetailText() const;
	virtual FString Describe() const override;

	/** Tests: a crew log reached this machine (as the raid state tells it). */
	void ShowLore(const FPrimaryAssetId& Entry, bool bNew);

	/** Hides it now (the timer's end; tests). */
	void Hide();

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EntryText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailText;

private:
	/** Follows the game state's raid state (it comes late on clients). */
	void BindRaid();

	TWeakObjectPtr<UHollowRaidStateComponent> Raid;
	FDelegateHandle LoreHandle;
	FDelegateHandle SourcesHandle;
	FTimerHandle HideTimer;
	bool bShown = false;
};
