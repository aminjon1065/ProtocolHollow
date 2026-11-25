#pragma once

#include "Visor/HollowVisorElement.h"

#include "HollowControlsHintWidget.generated.h"

class UHollowControlsHintSet;
class UHollowObjectiveModel;
class UTextBlock;

/**
 * The visor's controls cheat sheet (UX-01, ADR-0042): "[H] CONTROLS" over one line per action, with each action's
 * key on the current device ("LMB  Cutter", "RT  Cutter") and the player's class's own label. H / View folds it; the
 * choice is kept in the game's settings. Rows for tools and gadgets only show in a raid. Event driven, never ticks.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowControlsHintWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	FText GetTitleText() const;
	FText GetListText() const;
	/** Unfolded (the rows show) or folded to the title. */
	bool IsUnfolded() const;

	/** Tests: these rows instead of UHollowUISettings::ControlsHint. */
	void SetRowsForTests(const UHollowControlsHintSet* InRows);

	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ListText;

private:
	void Refresh();

	UPROPERTY(Transient)
	TObjectPtr<const UHollowControlsHintSet> Rows;

	TWeakObjectPtr<UHollowObjectiveModel> Objective;
	FDelegateHandle InputTypeHandle;
	FDelegateHandle KeysHandle;
	FDelegateHandle ClassHandle;
	FDelegateHandle ObjectiveHandle;
	FDelegateHandle SettingsHandle;
	/** The list pads plain strings into columns, so a new language builds it again (LOC-01). */
	FDelegateHandle LanguageHandle;
};
