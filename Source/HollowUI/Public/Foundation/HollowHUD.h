#pragma once

#include "GameFramework/HUD.h"

#include "HollowHUD.generated.h"

class UCommonActivatableWidget;
class UHollowDebriefWidget;
struct FHollowRaidReport;
class UHollowPrimaryLayout;
class UHollowVisorContext;
class UInputMappingContext;

/**
 * Local player's UI owner (ADR-0013): creates the visor context and the primary layout, pushes the visor, applies the UI
 * mapping context. Exists only on machines with that local player; recreated with the HUD on every map.
 */
UCLASS()
class HOLLOWUI_API AHollowHUD : public AHUD
{
	GENERATED_BODY()

public:
	explicit AHollowHUD(const FObjectInitializer& ObjectInitializer);

	static AHollowHUD* Get(const APlayerController* Player);

	UHollowPrimaryLayout* GetPrimaryLayout() const { return Layout; }
	UHollowVisorContext* GetVisorContext() const { return VisorContext; }

	/** Pushes the system menu on the Menu layer unless one is already active there. */
	UCommonActivatableWidget* OpenSystemMenu();

	/** Shows a resolved raid on the Menu layer (S3-03): the host's result, a lost host, an abandoned raid. */
	UHollowDebriefWidget* ShowDebrief(const FHollowRaidReport& Report);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleRaidReport(const FHollowRaidReport& Report);

	UPROPERTY(Transient)
	TObjectPtr<UHollowPrimaryLayout> Layout;

	UPROPERTY(Transient)
	TObjectPtr<UHollowVisorContext> VisorContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> UIMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UHollowDebriefWidget> Debrief;

	FDelegateHandle RaidReportHandle;
};
