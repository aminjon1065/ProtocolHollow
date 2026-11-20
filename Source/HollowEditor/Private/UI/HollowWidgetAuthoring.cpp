#include "UI/HollowWidgetAuthoring.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "HollowEditorLog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWidgetAuthoring)

namespace HollowWidgetAuthoringPrivate
{
	FSlateChildSize ChildSize(const float Fill)
	{
		FSlateChildSize Size(Fill > 0.f ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic);
		Size.Value = Fill > 0.f ? Fill : 1.f;
		return Size;
	}
}

UWidget* UHollowWidgetAuthoring::AddWidget(UWidgetBlueprint* Blueprint, const TSubclassOf<UWidget> WidgetClass, const FName Name, const FName Parent)
{
	if (!Blueprint || !Blueprint->WidgetTree || !WidgetClass || Name.IsNone() || Blueprint->WidgetTree->FindWidget(Name))
	{
		UE_LOG(LogHollowEditor, Error, TEXT("AddWidget %s to %s: no blueprint or class, or the name is taken."), *Name.ToString(), *GetNameSafe(Blueprint));
		return nullptr;
	}
	UPanelWidget* ParentPanel = nullptr;
	if (!Parent.IsNone())
	{
		ParentPanel = Cast<UPanelWidget>(Blueprint->WidgetTree->FindWidget(Parent));
		if (!ParentPanel)
		{
			UE_LOG(LogHollowEditor, Error, TEXT("AddWidget %s to %s: %s is not a panel in the tree."), *Name.ToString(), *Blueprint->GetName(), *Parent.ToString());
			return nullptr;
		}
	}
	else if (Blueprint->WidgetTree->RootWidget)
	{
		UE_LOG(LogHollowEditor, Error, TEXT("AddWidget %s to %s: the tree already has a root."), *Name.ToString(), *Blueprint->GetName());
		return nullptr;
	}

	Blueprint->Modify();
	UWidget* Widget = Blueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, Name);
	Widget->bIsVariable = true;   // C++ binds widgets by name (BindWidget)
	if (ParentPanel)
	{
		ParentPanel->AddChild(Widget);
	}
	else
	{
		Blueprint->WidgetTree->RootWidget = Widget;
	}
	Blueprint->OnVariableAdded(Widget->GetFName());
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return Widget;
}

UWidget* UHollowWidgetAuthoring::FindWidget(UWidgetBlueprint* Blueprint, const FName Name)
{
	return Blueprint && Blueprint->WidgetTree ? Blueprint->WidgetTree->FindWidget(Name) : nullptr;
}

bool UHollowWidgetAuthoring::SetCanvasSlot(UWidget* Widget, const FVector2D Anchor, const FVector2D Alignment, const FVector2D Position, const FVector2D Size,
	const bool bAutoSize)
{
	UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
	if (!Slot)
	{
		return false;
	}
	Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
	Slot->SetAlignment(Alignment);
	Slot->SetPosition(Position);
	Slot->SetSize(Size);
	Slot->SetAutoSize(bAutoSize);
	return true;
}

bool UHollowWidgetAuthoring::SetBoxSlot(UWidget* Widget, const EHorizontalAlignment HorizontalAlignment, const EVerticalAlignment VerticalAlignment,
	const FMargin Padding, const float Fill)
{
	using namespace HollowWidgetAuthoringPrivate;
	UPanelSlot* Slot = Widget ? Widget->Slot : nullptr;
	if (UOverlaySlot* Overlay = Cast<UOverlaySlot>(Slot))
	{
		Overlay->SetHorizontalAlignment(HorizontalAlignment);
		Overlay->SetVerticalAlignment(VerticalAlignment);
		Overlay->SetPadding(Padding);
		return true;
	}
	if (UHorizontalBoxSlot* Horizontal = Cast<UHorizontalBoxSlot>(Slot))
	{
		Horizontal->SetHorizontalAlignment(HorizontalAlignment);
		Horizontal->SetVerticalAlignment(VerticalAlignment);
		Horizontal->SetPadding(Padding);
		Horizontal->SetSize(ChildSize(Fill));
		return true;
	}
	if (UVerticalBoxSlot* Vertical = Cast<UVerticalBoxSlot>(Slot))
	{
		Vertical->SetHorizontalAlignment(HorizontalAlignment);
		Vertical->SetVerticalAlignment(VerticalAlignment);
		Vertical->SetPadding(Padding);
		Vertical->SetSize(ChildSize(Fill));
		return true;
	}
	return false;
}

bool UHollowWidgetAuthoring::Compile(UWidgetBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return false;
	}
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		UE_LOG(LogHollowEditor, Error, TEXT("%s does not compile."), *Blueprint->GetName());
		return false;
	}
	return true;
}
