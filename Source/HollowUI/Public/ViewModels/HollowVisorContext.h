#pragma once

#include "CommonInputTypeEnum.h"
#include "UObject/Object.h"

#include "HollowVisorContext.generated.h"

class AController;
class AHollowExoFrameCharacter;
class APawn;
class APlayerController;
class UHollowAbilitySystemComponent;
class UHollowPromptModel;
class UHollowToolTargetModel;
class UHollowVoiceComponent;

/**
 * What the visor of one local player shows comes from here (ADR-0013): it follows the player's pawn and its ability
 * system (clients get the ability system with the player state, after possession) and owns the visor models.
 * Plain C++, no MVVM plugin; widgets subscribe to native delegates, nothing ticks.
 */
UCLASS()
class HOLLOWUI_API UHollowVisorContext : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnHollowVisorSourcesChanged);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHollowInputTypeChanged, ECommonInputType);

	/** Starts following Player. Visor tuning comes from UHollowUISettings. */
	void Initialize(APlayerController* InPlayer);
	void Deinitialize();

	/** The context of the player's HUD (AHollowHUD), if any. */
	static UHollowVisorContext* Get(const APlayerController* Player);

	APlayerController* GetPlayer() const;
	AHollowExoFrameCharacter* GetCharacter() const;
	/** Null until the pawn is the avatar of its ability system. */
	UHollowAbilitySystemComponent* GetAbilitySystem() const;
	/** Current input device type (mouse and keyboard without a local player, e.g. in tests). */
	ECommonInputType GetInputType() const;

	UHollowPromptModel* GetPrompt() const { return Prompt; }
	UHollowToolTargetModel* GetToolTarget() const { return ToolTarget; }

	/**
	 * The player's voice (S4-07): OnVoiceActivityChanged drives the microphone indicator (drawn by the S4-08 visor);
	 * who else is talking comes from UHollowVoiceTalker::OnTalkingChanged.
	 */
	UHollowVoiceComponent* GetVoice() const;

	/** The pawn or its ability system changed: rebind. */
	FOnHollowVisorSourcesChanged OnSourcesChanged;
	FOnHollowInputTypeChanged OnInputTypeChanged;

	virtual UWorld* GetWorld() const override;

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** Clients learn possession here (the pawn's controller replicates). */
	UFUNCTION()
	void HandlePawnControllerChanged(APawn* Pawn, AController* Controller);

	void SetCharacter(AHollowExoFrameCharacter* NewCharacter);
	void HandleAbilitySystemChanged(UHollowAbilitySystemComponent* NewAbilitySystem);

	UPROPERTY(Transient)
	TObjectPtr<UHollowPromptModel> Prompt;

	UPROPERTY(Transient)
	TObjectPtr<UHollowToolTargetModel> ToolTarget;

	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<AHollowExoFrameCharacter> Character;
	TWeakObjectPtr<UHollowAbilitySystemComponent> AbilitySystem;
	FDelegateHandle AbilitySystemHandle;
	FDelegateHandle InputTypeHandle;
};
