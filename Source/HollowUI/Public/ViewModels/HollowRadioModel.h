#pragma once

#include "Radio/HollowRadioRules.h"
#include "UObject/Object.h"
#include "ViewModels/HollowResonanceTierWatch.h"

#include "HollowRadioModel.generated.h"

class UHollowAbilitySystemComponent;
class UHollowRaidStateComponent;
class UHollowRadioScript;
class UHollowVisorContext;
struct FHollowRaidNetState;

/** What the visor's subtitle shows now. */
struct HOLLOWUI_API FHollowRadioSubtitle
{
	bool bShown = false;
	EHollowRadioSpeaker Speaker = EHollowRadioSpeaker::Handler;
	FText SpeakerName;
	FText Text;
};

/**
 * The handler's radio on one machine (NAR-01a, ADR-0049), owned by the visor context. It gathers FHollowRadioFacts from
 * what this machine already knows (the raid's replicated state, the objective's facts, the player's Resonance tier,
 * the sector's manifest, two helmet sensors), turns their changes into triggers (HollowRadioRules::TriggersFor) and
 * plays the cues of DA_RadioScript through FHollowRadioQueue: subtitles on the visor, a click as a transmission opens
 * and closes, Profile cues remembered in the player's profile. Nothing crosses the network; nothing ticks: a timer
 * wakes the queue when a line ends, and the sensors look twice a second only while a core or the stalker is unseen.
 */
UCLASS()
class HOLLOWUI_API UHollowRadioModel : public UObject
{
	GENERATED_BODY()

public:
	/** The subtitle changed (a line went on or off the air). */
	DECLARE_MULTICAST_DELEGATE(FOnHollowRadioChanged);

	void Initialize(UHollowVisorContext& InContext);
	void Deinitialize();

	const FHollowRadioSubtitle& GetSubtitle() const { return Subtitle; }
	const FHollowRadioFacts& GetFacts() const { return Facts; }
	const UHollowRadioScript* GetScript() const;
	const FHollowRadioQueue& GetQueue() const { return Queue; }

	/** Development (Hollow.Radio.Fire) and tests: fires Trigger now; bForce ignores its scope and cooldown. True if queued. */
	bool Fire(const FGameplayTag& Trigger, bool bForce = false);

	/** Development (Hollow.Radio.Reset): forgets what was heard (this raid's memory; the profile's is the caller's). */
	void Forget();

	/** Re-reads every fact now and fires what changed (tests after changing state by hand). */
	void Refresh();

	/** Moves the radio to the clock's time (the timer does it in a game). */
	void Update();

	/** One line for Hollow.UI.Dump. */
	FString Describe() const;

	/** Tests: this script instead of UHollowUISettings::RadioScript. */
	void SetScriptForTests(const UHollowRadioScript* InScript);
	/** Tests: Profile cues are remembered here instead of the local player's profile (automation worlds have none). */
	void SetHeardForTests(TSet<FName>* InHeard);
	/** Tests: the time the radio runs on (seconds); unset: the world's. */
	void SetClockForTests(TFunction<double()> InClock);

	FOnHollowRadioChanged OnRadioChanged;

	virtual UWorld* GetWorld() const override;

private:
	void BindSources();
	void HandleRaidState(const FHollowRaidNetState& OldState, const FHollowRaidNetState& NewState);
	void HandleLoreFound(const FPrimaryAssetId& Entry, bool bNew);
	void GatherFacts(FHollowRadioFacts& OutFacts) const;
	/** The sensors: a core still in its vault, the Echo-Stalker, in view and in line of sight. */
	void Sense();
	bool HasLineOfSight(const FVector& From, const AActor& Target) const;
	void UpdateSensorTimer();
	void ApplyEvents(const FHollowRadioEvents& Events);
	void Schedule();
	void UpdateSubtitle();
	void PlayClick(const FGameplayTag& Sound) const;
	bool HasHeard(FName CueId) const;
	void MarkHeard(FName CueId);
	double Now() const;

	TWeakObjectPtr<UHollowVisorContext> Context;
	TWeakObjectPtr<UHollowRaidStateComponent> RaidState;
	TWeakObjectPtr<UHollowAbilitySystemComponent> BoundAbilities;
	FHollowResonanceTierWatch Tier;

	UPROPERTY(Transient)
	TObjectPtr<const UHollowRadioScript> Script;

	FHollowRadioQueue Queue;
	FHollowRadioFacts Facts;
	FHollowRadioSubtitle Subtitle;
	/** The sensors' finds of this raid (they stay found). */
	FGuid SensedRaid;
	bool bCoreSighted = false;
	bool bStalkerSighted = false;
	/** NAR-01b: crew logs new to this player downloaded in raid LoreRaid. */
	FGuid LoreRaid;
	int32 NewLore = 0;

	TSet<FName>* TestHeard = nullptr;
	TFunction<double()> TestClock;

	FDelegateHandle SourcesHandle;
	FDelegateHandle ObjectiveHandle;
	FDelegateHandle RaidHandle;
	FDelegateHandle LoreHandle;
	FTimerHandle WakeTimer;
	FTimerHandle SensorTimer;
};
