#pragma once

#include "CommonUserWidget.h"

#include "HollowVisorElement.generated.h"

class UHollowVisorContext;

/** Part of the visor bound to the owning player's visor context while constructed; event driven, never ticks. */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowVisorElement : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Binds to a context explicitly (tests, previews); otherwise the owning player's HUD context is used on construct. */
	void SetContext(UHollowVisorContext* InContext);
	UHollowVisorContext* GetContext() const { return Context.Get(); }

	/** One line of what it shows, for Hollow.UI.Dump (empty: nothing to say). */
	virtual FString Describe() const { return FString(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual void BindContext(UHollowVisorContext& InContext) {}
	virtual void UnbindContext(UHollowVisorContext& InContext) {}

private:
	TWeakObjectPtr<UHollowVisorContext> Context;
};
