#pragma once

#include "Noise/HollowNoiseTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"

#include "HollowNoiseSubsystem.generated.h"

class UHollowNoiseConfig;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowNoiseReported, const FHollowNoiseReport&);

/**
 * The one entry point of noise -> AI (CLAUDE.md sec. 6, Docs/Architecture/03 sec. 3.8, ADR-0018), server only: a
 * source reports what it is and where; the rule of its tag in UHollowNoiseConfig gives loudness, range and rate;
 * the result goes to UAISense_Hearing and to OnNoiseReported (heatmap and director, later). Sources never pick
 * numbers themselves.
 */
UCLASS()
class HOLLOWCORE_API UHollowNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowNoiseSubsystem* Get(const UObject* WorldContext);

	/**
	 * Server only: reports the noise unless its tag has no rule, it is silent, or the same instigator made the same
	 * noise less than the rule's MinInterval ago. True if it went out.
	 */
	bool ReportNoise(const FHollowNoiseEvent& Event);

	const UHollowNoiseConfig* GetConfig() const { return Config; }
	/** Replaces the project's config (tests). */
	void SetConfig(const UHollowNoiseConfig* InConfig);

	/** Every noise that went out, after its rule. */
	FOnHollowNoiseReported OnNoiseReported;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UHollowNoiseConfig> Config;

	/** Last time each instigator made each rate-limited noise. */
	TMap<TPair<FObjectKey, FGameplayTag>, double> LastReported;

	/** Tags without a rule, already warned about. */
	TArray<FGameplayTag> UnknownTags;
};
