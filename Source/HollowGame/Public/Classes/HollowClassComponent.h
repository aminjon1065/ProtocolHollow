#pragma once

#include "Abilities/HollowAbilitySet.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowClassComponent.generated.h"

class APlayerState;
class UHollowAbilitySystemComponent;
class UHollowClassDefinition;

/**
 * The class a player plays (S6-01, ADR-0034), on its AHollowPlayerState: replicated to everyone (push model). The
 * server alone changes it (the hub's Server_SelectClass, the raid's loadout snapshot, seamless travel) and, while the
 * class is played, keeps its ability set, its attribute effect and its Hollow.Class tag on the player's ability system;
 * a new class takes the old one's off first. Clients follow OnClassChanged (the visor's colour).
 */
UCLASS(ClassGroup = (Hollow))
class HOLLOWGAME_API UHollowClassComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowClassChanged, const FPrimaryAssetId&);

	explicit UHollowClassComponent(const FObjectInitializer& ObjectInitializer);

	/** The component on PlayerState, if any. */
	static UHollowClassComponent* Get(const APlayerState* PlayerState);

	const FPrimaryAssetId& GetClassId() const { return ClassId; }

	/** The class's definition (loaded once, a tiny data asset), or null without a class. */
	const UHollowClassDefinition* GetDefinition() const;

	/**
	 * Server: the player plays Class now; its grants replace the previous class's. False (nothing changes) off the
	 * server or for anything that is not a class; true and nothing more for the class played already.
	 */
	bool SetClass(const FPrimaryAssetId& Class);

	/** "DA_Class_Engineer (Hollow.Class.Engineer)" for logs and dumps. */
	FString Describe() const;

	/** The class changed: on the server at once, on clients when it arrives. */
	FOnHollowClassChanged OnClassChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class FHollowClassSwitchTest;

	UFUNCTION()
	void OnRep_ClassId();

	UHollowAbilitySystemComponent* GetAbilitySystem() const;
	void Grant(const UHollowClassDefinition& Definition);
	void Revoke();

	UPROPERTY(ReplicatedUsing = OnRep_ClassId)
	FPrimaryAssetId ClassId;

	/** The definition of ClassId once loaded. */
	UPROPERTY(Transient)
	mutable TObjectPtr<const UHollowClassDefinition> Definition;

	// What the server granted for the class played, taken back exactly when it changes.
	FHollowAbilitySetHandles GrantedAbilities;
	FActiveGameplayEffectHandle GrantedEffect;
	FGameplayTag GrantedTag;
};
