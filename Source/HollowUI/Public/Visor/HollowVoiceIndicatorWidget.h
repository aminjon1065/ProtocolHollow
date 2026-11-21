#pragma once

#include "Visor/HollowVisorElement.h"

#include "HollowVoiceIndicatorWidget.generated.h"

class UHollowVoiceComponent;
class UHollowVoiceTalker;
class UProgressBar;
class UTextBlock;
enum class EHollowVoiceMode : uint8;

/**
 * Visor telemetry of the voice (S4-08, from S4-07): the player's mode ("MIC PTT"), whether the voice goes out and how
 * loud, and who else is talking on this machine. Follows the voice component's and the talkers' events.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowVoiceIndicatorWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	FText GetModeText() const;
	FText GetTalkingText() const;
	bool IsTransmitting() const { return bTransmitting; }
	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ModeText;

	/** The microphone level while the voice goes out. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> LevelBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TalkingText;

private:
	void HandleActivity(bool bInTransmitting, uint8 InLevel);
	void HandleMode(EHollowVoiceMode Mode);
	void HandleTalking(const UHollowVoiceTalker* Talker, bool bTalking);
	void Refresh();

	TWeakObjectPtr<UHollowVoiceComponent> Voice;
	TArray<TWeakObjectPtr<const UHollowVoiceTalker>> Talking;
	FDelegateHandle ActivityHandle;
	FDelegateHandle ModeHandle;
	FDelegateHandle TalkingHandle;
	bool bTransmitting = false;
	uint8 Level = 0;
};
