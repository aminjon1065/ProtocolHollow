#include "Foundation/HollowButton.h"

#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowButton)

void UHollowButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (LabelText)
	{
		LabelText->SetText(Label);
	}
}
