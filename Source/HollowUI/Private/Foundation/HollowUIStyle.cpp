#include "Foundation/HollowUIStyle.h"

#include "Foundation/HollowUISettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowUIStyle)

const UHollowUIStyle* UHollowUIStyle::Get()
{
	return GetDefault<UHollowUISettings>()->UIStyle.LoadSynchronous();
}

const FHollowBadgeLook* UHollowUIStyle::FindBadge(const EHollowHubBadgeKind Kind) const
{
	return Badges.FindByPredicate([Kind](const FHollowBadgeLook& Look) { return Look.Kind == Kind; });
}
