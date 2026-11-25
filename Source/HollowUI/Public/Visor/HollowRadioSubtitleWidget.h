#pragma once

#include "Visor/HollowVisorElement.h"

#include "HollowRadioSubtitleWidget.generated.h"

class UHollowRadioModel;
class UTextBlock;

/**
 * The radio's subtitle on the visor (NAR-01a, ADR-0049): the speaker's call sign over the line on the air, low in the
 * middle of the view, from the visor context's UHollowRadioModel. Hidden while the radio is quiet. Its texts are the
 * script's string table entries, so a language switch redraws the line on the air at once. Event driven, never ticks.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowRadioSubtitleWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	bool IsShown() const;
	FText GetSpeakerText() const;
	FText GetLineText() const;
	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SpeakerText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LineText;

private:
	void Refresh();

	TWeakObjectPtr<UHollowRadioModel> Radio;
	FDelegateHandle RadioHandle;
	FDelegateHandle ClassHandle;
};
