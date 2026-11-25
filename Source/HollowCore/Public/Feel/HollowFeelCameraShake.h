#pragma once

#include "Camera/CameraShakeBase.h"
#include "Feel/HollowFeelTypes.h"

#include "HollowFeelCameraShake.generated.h"

/** The shape of every game-feel shake (GP-01c): HollowFeelRules::Sample of the params it was set up with. */
UCLASS()
class HOLLOWCORE_API UHollowFeelShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	void Setup(const FHollowShakeParams& InShake, uint32 InSeed);
	const FHollowShakeParams& GetShake() const { return Shake; }
	float GetElapsed() const { return Elapsed; }

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;

	void Evaluate(FCameraShakePatternUpdateResult& OutResult) const;

	FHollowShakeParams Shake;
	uint32 Seed = 0;
	float Elapsed = 0.f;
	bool bStopped = false;
};

/**
 * The one camera shake class of game feel (GP-01c, ADR-0046): UHollowFeelSubsystem sets each instance up from its
 * DA_FeelConfig entry as it starts (FAddCameraShakeParams::Initializer), so every shake is data, not a class.
 */
UCLASS()
class HOLLOWCORE_API UHollowFeelCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	explicit UHollowFeelCameraShake(const FObjectInitializer& ObjectInitializer);

	void Setup(const FHollowShakeParams& Shake, uint32 Seed);
	const UHollowFeelShakePattern* GetPattern() const;
};
