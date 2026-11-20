#include "Visor/HollowVisorElement.h"

#include "ViewModels/HollowVisorContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowVisorElement)

void UHollowVisorElement::SetContext(UHollowVisorContext* InContext)
{
	if (Context.Get() == InContext)
	{
		return;
	}
	if (UHollowVisorContext* Old = Context.Get())
	{
		UnbindContext(*Old);
	}
	Context = InContext;
	if (InContext)
	{
		BindContext(*InContext);
	}
}

void UHollowVisorElement::NativeConstruct()
{
	Super::NativeConstruct();
	if (!Context.IsValid())
	{
		SetContext(UHollowVisorContext::Get(GetOwningPlayer()));
	}
}

void UHollowVisorElement::NativeDestruct()
{
	SetContext(nullptr);
	Super::NativeDestruct();
}
