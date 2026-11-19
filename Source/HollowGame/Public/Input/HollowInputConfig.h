#pragma once

#include "Engine/DataAsset.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "HollowGameLog.h"

#include "HollowInputConfig.generated.h"

class UInputAction;

/** One input action addressed by a Hollow.Input.* tag. */
USTRUCT(BlueprintType)
struct FHollowInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Hollow.Input"))
	FGameplayTag InputTag;
};

/**
 * Tag -> input action table of an input context (e.g. DA_InputConfig_Raid). Code binds by tag, never by asset,
 * so remapping and new contexts are data changes. Ability actions are routed to the ability system by tag (S2-03).
 */
UCLASS(BlueprintType, Const)
class HOLLOWGAME_API UHollowInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Native (non-ability) action for the tag, or nullptr when the config does not map it. */
	const UInputAction* FindNativeAction(const FGameplayTag& InputTag) const;

	/** Binds a native action by tag. Logs and returns false when the tag is not mapped (configuration error). */
	template <class UserClass, typename FuncType>
	bool BindNativeAction(UEnhancedInputComponent& InputComponent, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent,
		UserClass* Object, FuncType Func) const
	{
		const UInputAction* Action = FindNativeAction(InputTag);
		if (!Action)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("%s: no input action mapped to %s."), *GetName(), *InputTag.ToString());
			return false;
		}
		InputComponent.BindAction(Action, TriggerEvent, Object, Func);
		return true;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FHollowInputAction> NativeActions;

	/** Pressed / released go to UHollowAbilitySystemComponent::AbilityInputTag*, which finds abilities by the same tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FHollowInputAction> AbilityActions;
};
