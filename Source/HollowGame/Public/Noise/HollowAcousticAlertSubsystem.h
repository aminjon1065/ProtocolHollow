#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "HollowAcousticAlertSubsystem.generated.h"

struct FHollowNoiseAnswer;

/**
 * POLISH-01 (ADR-0053), server only: when an enemy answers a player's noise (UHollowNoiseSubsystem::OnAcousticAlert,
 * already limited to one per player per DA_NoiseConfig's AcousticAlertCooldown), the player who made it is told on its
 * own machine (AHollowPlayerController::Client_AcousticAlert), so the squad always knows why a hunt began.
 */
UCLASS()
class HOLLOWGAME_API UHollowAcousticAlertSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowAcousticAlertSubsystem* Get(const UObject* WorldContext);

	/** Alerts sent in this world (tests, the smoke logs). */
	int32 GetAlertsSent() const { return AlertsSent; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandleAlert(const FHollowNoiseAnswer& Answer);

	FDelegateHandle AlertHandle;
	int32 AlertsSent = 0;
};
