#pragma once

#include "NativeGameplayTags.h"

/**
 * Compile-time enforcement of the GameplayTag namespace rule (CLAUDE.md sec. 4):
 * every native tag lives under "Hollow." - the single exception is "GameplayCue.Hollow.",
 * because GAS only discovers cues under the "GameplayCue." root.
 */
namespace HollowTagRules
{
	/** constexpr prefix test for narrow string literals (native tag names are narrow literals). */
	constexpr bool StartsWith(const char* Text, const char* Prefix)
	{
		for (; *Prefix != '\0'; ++Text, ++Prefix)
		{
			if (*Text != *Prefix)
			{
				return false;
			}
		}
		return true;
	}

	constexpr bool IsAllowedNamespace(const char* Tag)
	{
		return StartsWith(Tag, "Hollow.") || StartsWith(Tag, "GameplayCue.Hollow.");
	}
}

/**
 * Defines a native GameplayTag declared with UE_DECLARE_GAMEPLAY_TAG_EXTERN.
 * Use it instead of UE_DEFINE_GAMEPLAY_TAG_COMMENT in every Hollow module (.cpp files only).
 */
#define HOLLOW_DEFINE_GAMEPLAY_TAG(TagName, Tag, Comment) \
	static_assert(HollowTagRules::IsAllowedNamespace(Tag), "Native GameplayTag '" Tag "' must live under 'Hollow.' or 'GameplayCue.Hollow.' (CLAUDE.md section 4)"); \
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TagName, Tag, Comment)
