#include "Menus/HollowSettingsLayout.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSettingsLayout)

const FHollowSettingsTabSpec* UHollowSettingsLayout::FindTab(const FName Id) const
{
	return Tabs.FindByPredicate([Id](const FHollowSettingsTabSpec& Tab) { return Tab.Id == Id; });
}
