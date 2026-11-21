#pragma once

#include "Components/ActorComponent.h"
#include "Enemies/HollowEnemyMontages.h"
#include "GameplayTagContainer.h"

#include "HollowEnemyMontageComponent.generated.h"

class UAbilitySystemComponent;
class UHollowEnemyDefinition;
struct FStreamableHandle;

/**
 * Plays an enemy's montages (UHollowEnemyDefinition::Montages, ADR-0027) on every machine from what already reaches
 * it: the wind-up cue of an attack, Hollow.State.Stunned, the vent state. Nothing more is replicated. Assigned montages
 * load asynchronously at begin play; an empty slot plays nothing and says nothing. Never ticks.
 */
UCLASS(ClassGroup = Hollow)
class HOLLOWAI_API UHollowEnemyMontageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHollowEnemyMontageComponent();

	static UHollowEnemyMontageComponent* Get(const AActor* Owner);

	/** Whose montages (the owner's definition; tests give their own). */
	void SetDefinition(const UHollowEnemyDefinition* InDefinition);

	/**
	 * Plays Moment's montage if the definition has one. FitSeconds > 0 (an attack): the impact section lands then.
	 * Returns how long it plays, 0 if it does not (empty slot, still loading, no animation instance).
	 */
	float Play(EHollowEnemyMontage Moment, float FitSeconds = 0.f);
	void Stop(EHollowEnemyMontage Moment);

	/** The montage last started (tests, dumps). */
	EHollowEnemyMontage GetLastPlayed() const { return LastPlayed; }
	int32 GetPlays() const { return Plays; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleStunned(FGameplayTag Tag, int32 Count);

	UPROPERTY(Transient)
	TObjectPtr<const UHollowEnemyDefinition> Definition;

	TWeakObjectPtr<UAbilitySystemComponent> StunSource;
	FDelegateHandle StunnedHandle;
	TSharedPtr<FStreamableHandle> Loading;
	EHollowEnemyMontage LastPlayed = EHollowEnemyMontage::Melee;
	int32 Plays = 0;
	bool bWarnedNoAnimation = false;
};
