#pragma once

#include "GameFramework/HUD.h"

#include "HollowHUD.generated.h"

class UCommonActivatableWidget;
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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UHollowPrimaryLayout> Layout;

	UPROPERTY(Transient)
	TObjectPtr<UHollowVisorContext> VisorContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> UIMappingContext;
};
