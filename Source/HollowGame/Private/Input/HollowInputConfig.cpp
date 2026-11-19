#include "Input/HollowInputConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowInputConfig)

const UInputAction* UHollowInputConfig::FindNativeAction(const FGameplayTag& InputTag) const
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	for (const FHollowInputAction& Entry : NativeActions)
	{
		if (Entry.InputAction && Entry.InputTag == InputTag)
		{
			return Entry.InputAction;
		}
	}
	return nullptr;
}
