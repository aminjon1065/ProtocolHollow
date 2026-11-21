#pragma once

#include "CommonUserWidget.h"
#include "Hub/HollowHubScreenRules.h"

#include "HollowHubRowWidget.generated.h"

class APlayerController;
class UHollowButton;
class UTextBlock;

/**
 * One line of a hub screen's list (W_HubRow, S5-04): name, detail and up to three buttons. A screen reuses its rows
 * (SetRow), so a button keeps the keyboard / gamepad focus while the profile changes under it. A button the rules
 * would refuse stays focusable but greyed out, and tells its reason when focused (and as its tooltip).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowHubRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowHubRowAction, UHollowHubRowWidget* /*Row*/, int32 /*ActionIndex*/);

	void SetRow(const FHollowHubRow& InRow);
	const FHollowHubRow& GetRow() const { return Row; }

	/** The button of action Index (null beyond MaxActions). */
	UHollowButton* GetActionButton(int32 Index) const;

	/** Its first shown button (null for a header or a note). */
	UHollowButton* GetFirstActionButton() const;

	/** The action whose button has Player's focus (any user's without a player), or INDEX_NONE. */
	int32 GetFocusedAction(APlayerController* Player) const;

	/** A button was pressed (an available one: CommonUI ignores presses on greyed-out buttons). */
	FOnHollowHubRowAction OnActionClicked;

	/** A button got the focus or the mouse. */
	FOnHollowHubRowAction OnActionFocused;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHollowButton> FirstAction;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHollowButton> SecondAction;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHollowButton> ThirdAction;

	/** Name colours (create_hub_ui_assets.py). */
	UPROPERTY(EditDefaultsOnly, Category = "Look")
	FLinearColor NameColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Look")
	FLinearColor HighlightColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Look")
	FLinearColor HeaderColor = FLinearColor::White;

private:
	void HandleClicked(int32 Index);
	void HandleFocused(int32 Index);

	FHollowHubRow Row;
};
