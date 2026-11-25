#pragma once

#include "Damage/HollowDamageMath.h"
#include "Visor/HollowVisorElement.h"

#include "HollowHitMarkerWidget.generated.h"

class UHollowHandsComponent;
class UImage;

/**
 * The visor's hit marker round the crosshair (GP-01c, ADR-0046): the owner's shot or blow hit, as its machine sees it
 * (UHollowHandsComponent::OnHitMarker, no round trip). A plain mark for a hit, a dull smaller one when armour took most
 * of it, a bright larger one on an exposed target (DA_VisorStyle), each with its own tick in the helmet; it goes after
 * HitMarkerSeconds. Event driven, never ticks.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowHitMarkerWidget : public UHollowVisorElement
{
	GENERATED_BODY()

public:
	explicit UHollowHitMarkerWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows Kind now (the hands' marker; tests call it too). None does nothing. */
	void Show(EHollowHitKind Kind);

	EHollowHitKind GetShown() const { return Shown; }
	bool IsShowing() const { return bShowing; }
	int32 GetShownCount() const { return ShownCount; }
	virtual FString Describe() const override;

protected:
	virtual void BindContext(UHollowVisorContext& InContext) override;
	virtual void UnbindContext(UHollowVisorContext& InContext) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Marker;

private:
	void Rebind();
	void Unbind();
	void Hide();

	TWeakObjectPtr<UHollowHandsComponent> Hands;
	FDelegateHandle HandsHandle;
	FDelegateHandle SourcesHandle;
	FTimerHandle HideTimer;
	EHollowHitKind Shown = EHollowHitKind::None;
	int32 ShownCount = 0;
	bool bShowing = false;
};
