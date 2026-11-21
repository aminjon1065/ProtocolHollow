#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"

#include "HollowPlayerState.generated.h"

class UHollowAbilitySystemComponent;
class UHollowHealthSet;
class UHollowResonanceSet;
class UHollowSurvivalSet;
class UHollowToolSet;
class UHollowVoiceTalker;

/**
 * Player state of Protocol: Hollow. Owns the player's ability system and attribute sets, so they survive pawn death
 * and respawn (Docs/Architecture/02 sec. 2.2). Replication mode Mixed: full effect data to the owner, minimal to others.
 */
UCLASS()
class HOLLOWGAME_API AHollowPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	explicit AHollowPlayerState(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UHollowAbilitySystemComponent* GetHollowAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** How this player's voice sounds on this machine (S4-07): from the helmet of a living body, else on the radio. */
	UHollowVoiceTalker* GetVoiceTalker() const { return VoiceTalker; }

protected:
	/** The voice backend knows a player by its unique net id: the talker registers once it is known (server or client). */
	virtual void OnSetUniqueId() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Abilities")
	TObjectPtr<UHollowAbilitySystemComponent> AbilitySystemComponent;

	// Attribute sets are default subobjects: the ability system registers them when it initializes.
	UPROPERTY()
	TObjectPtr<UHollowHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<UHollowSurvivalSet> SurvivalSet;

	UPROPERTY()
	TObjectPtr<UHollowResonanceSet> ResonanceSet;

	UPROPERTY()
	TObjectPtr<UHollowToolSet> ToolSet;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Voice")
	TObjectPtr<UHollowVoiceTalker> VoiceTalker;
};
