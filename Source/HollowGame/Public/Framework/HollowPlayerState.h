#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"

#include "HollowPlayerState.generated.h"

class UHollowAbilitySystemComponent;
class UHollowClassComponent;
class UHollowHealthSet;
class UHollowInventoryComponent;
class UHollowResonanceSet;
class UHollowSurvivalSet;
class UHollowToolSet;
class UHollowGearSet;
class UHollowWeaponSet;
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

	/** The raid inventory (ADR-0030): the accepted loadout; its items grant the tools and consumables on this ability system. */
	UHollowInventoryComponent* GetInventory() const { return Inventory; }

	/** The class this player plays (S6-01, ADR-0034), replicated to everyone. */
	UHollowClassComponent* GetClassComponent() const { return ClassComponent; }

	/** Seamless travel: the new player state plays the same class (the raid's loadout snapshot confirms it). */
	virtual void CopyProperties(APlayerState* PlayerState) override;

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

	/** What the player's gear changes (S6-04: the relics' multipliers). */
	UPROPERTY()
	TObjectPtr<UHollowGearSet> GearSet;

	/** GP-01: the weapon's magazine (owner only). */
	UPROPERTY()
	TObjectPtr<UHollowWeaponSet> WeaponSet;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Voice")
	TObjectPtr<UHollowVoiceTalker> VoiceTalker;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Inventory")
	TObjectPtr<UHollowInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Class")
	TObjectPtr<UHollowClassComponent> ClassComponent;
};
