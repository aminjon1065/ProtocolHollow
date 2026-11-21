#pragma once

#include "Foundation/HollowActivatableWidget.h"

#include "HollowSystemMenuWidget.generated.h"

class UCommonButtonBase;
enum class EHollowVoiceMode : uint8;

/**
 * System menu on the Menu layer (Esc / Start): resume, the session's actions (S4-06: invite friends through the Steam
 * overlay, start the raid, leave the session) and quit. A session button shows only where it applies
 * (HollowMenuRules::SystemMenuOptions). Never pauses the game, solo or co-op (parity, CLAUDE.md sec. 1). Back (Esc / B)
 * closes it; gamepad focus starts on Resume. The voice button cycles how the voice goes out (S4-07: push-to-talk,
 * open mic, muted), saved in the player's settings.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowSystemMenuWidget : public UHollowActivatableWidget
{
	GENERATED_BODY()

public:
	explicit UHollowSystemMenuWidget(const FObjectInitializer& ObjectInitializer);

	UCommonButtonBase* GetResumeButton() const { return ResumeButton; }
	UCommonButtonBase* GetInviteButton() const { return InviteButton; }
	UCommonButtonBase* GetStartRaidButton() const { return StartRaidButton; }
	UCommonButtonBase* GetLeaveButton() const { return LeaveButton; }
	UCommonButtonBase* GetVoiceButton() const { return VoiceButton; }

	/** The voice button's label for Mode ("Voice: Push-to-talk"...). */
	static FText VoiceLabel(EHollowVoiceMode Mode);

	/** Shows the session buttons that apply to Context (on activation: the local player's). */
	void ApplyContext(const struct FHollowMenuContext& Context);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> ResumeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> QuitButton;

	/** Session buttons (S4-06; optional so a tree from before them still compiles: create_ui_assets.py adds them). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> InviteButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> StartRaidButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> LeaveButton;

	/** Voice mode toggle (S4-07; optional for the same reason). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> VoiceButton;

private:
	void Resume();
	void Quit();
	void Invite();
	void StartRaid();
	void Leave();
	void CycleVoice();
	void RefreshVoiceLabel();
};
