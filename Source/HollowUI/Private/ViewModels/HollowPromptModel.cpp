#include "ViewModels/HollowPromptModel.h"

#include "Character/HollowExoFrameCharacter.h"
#include "Foundation/HollowUISettings.h"
#include "InputAction.h"
#include "Interaction/HollowInteractionComponent.h"
#include "Interfaces/HollowInteractable.h"
#include "ViewModels/HollowVisorContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowPromptModel)

void UHollowPromptModel::Initialize(UHollowVisorContext& InContext)
{
	Context = &InContext;
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	InputTypeHandle = InContext.OnInputTypeChanged.AddWeakLambda(this, [this](ECommonInputType)
	{
		RefreshKey();
		OnChanged.Broadcast();
	});
	KeysHandle = InContext.OnKeysChanged.AddWeakLambda(this, [this]
	{
		RefreshKey();
		OnChanged.Broadcast();
	});
	Rebind();
}

void UHollowPromptModel::Deinitialize()
{
	if (UHollowInteractionComponent* Old = Interaction.Get())
	{
		Old->OnFocusChanged.Remove(FocusHandle);
	}
	Interaction.Reset();
	if (UHollowVisorContext* Owner = Context.Get())
	{
		Owner->OnSourcesChanged.Remove(SourcesHandle);
		Owner->OnInputTypeChanged.Remove(InputTypeHandle);
		Owner->OnKeysChanged.Remove(KeysHandle);
	}
	Context.Reset();
}

void UHollowPromptModel::Rebind()
{
	const AHollowExoFrameCharacter* Character = Context.IsValid() ? Context->GetCharacter() : nullptr;
	UHollowInteractionComponent* NewInteraction = Character ? Character->GetInteractionComponent() : nullptr;
	if (Interaction.Get() == NewInteraction)
	{
		return;
	}
	if (UHollowInteractionComponent* Old = Interaction.Get())
	{
		Old->OnFocusChanged.Remove(FocusHandle);
	}
	Interaction = NewInteraction;
	AActor* CurrentFocus = nullptr;
	FHollowInteractionInfo Info;
	if (NewInteraction)
	{
		FocusHandle = NewInteraction->OnFocusChanged.AddUObject(this, &ThisClass::HandleFocusChanged);
		CurrentFocus = NewInteraction->GetFocusedActor();
		if (const IHollowInteractable* Interactable = Cast<IHollowInteractable>(CurrentFocus))
		{
			Info = Interactable->GetInteractionInfo(Character);
		}
	}
	HandleFocusChanged(CurrentFocus, Info);
}

void UHollowPromptModel::HandleFocusChanged(AActor* NewFocus, const FHollowInteractionInfo& Info)
{
	Focus = NewFocus;
	Prompt = NewFocus ? Info.Prompt : FText::GetEmpty();
	RefreshKey();
	OnChanged.Broadcast();
}

void UHollowPromptModel::RefreshKey()
{
	KeyLabel = Context.IsValid() ? Context->GetKeyLabel(GetDefault<UHollowUISettings>()->InteractAction.LoadSynchronous()) : FText::GetEmpty();
}
