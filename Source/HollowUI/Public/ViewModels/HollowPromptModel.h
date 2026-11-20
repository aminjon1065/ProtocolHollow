#pragma once

#include "UObject/Object.h"

#include "HollowPromptModel.generated.h"

class AActor;
class UHollowInteractionComponent;
class UHollowVisorContext;
struct FHollowInteractionInfo;

/** Interaction prompt of the visor: the focused interactable's text and the Interact key for the current device. */
UCLASS()
class HOLLOWUI_API UHollowPromptModel : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnHollowPromptChanged);

	void Initialize(UHollowVisorContext& InContext);
	void Deinitialize();

	bool IsVisible() const { return Focus.IsValid(); }
	AActor* GetFocus() const { return Focus.Get(); }
	const FText& GetPrompt() const { return Prompt; }
	/** Empty without a key for the current device (no local player, or the action is unmapped). */
	const FText& GetKeyLabel() const { return KeyLabel; }

	FOnHollowPromptChanged OnChanged;

private:
	void Rebind();
	void HandleFocusChanged(AActor* NewFocus, const FHollowInteractionInfo& Info);
	void RefreshKey();

	TWeakObjectPtr<UHollowVisorContext> Context;
	TWeakObjectPtr<UHollowInteractionComponent> Interaction;
	TWeakObjectPtr<AActor> Focus;
	FText Prompt;
	FText KeyLabel;
	FDelegateHandle SourcesHandle;
	FDelegateHandle InputTypeHandle;
	FDelegateHandle FocusHandle;
};
