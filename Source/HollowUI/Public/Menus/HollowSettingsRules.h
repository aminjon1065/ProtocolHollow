#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UHollowGameUserSettings;
enum class EHollowLanguage : uint8;
enum class EHollowSettingId : uint8;
struct FHollowSettingsTexts;

/**
 * What a row of the settings screen shows and what stepping it does (UI-02, ADR-0052). Every change goes through the
 * player's settings (UHollowGameUserSettings), applied at once and kept for the next launch, the same in the main menu,
 * the hub and a raid.
 */
namespace HollowSettingsRules
{
	/** Whether a setting is a choice (it wraps round) rather than a number on a range (it stops at its ends). */
	HOLLOWUI_API bool IsChoice(EHollowSettingId Setting);

	/** The value a row shows now. */
	HOLLOWUI_API FText ValueText(EHollowSettingId Setting, const UHollowGameUserSettings& Settings, const FHollowSettingsTexts& Texts);

	/**
	 * Moves a setting one step (Direction > 0: next / up, < 0: previous / down); returns whether it changed. Player's
	 * live voice takes a new voice mode at once (none in the main menu: the next session reads the kept one).
	 */
	HOLLOWUI_API bool Step(EHollowSettingId Setting, UHollowGameUserSettings& Settings, int32 Direction, APlayerController* Player);

	/** A language's name in itself ("English", "Russkiy" in Cyrillic), whatever language the texts show in. */
	HOLLOWUI_API FText LanguageName(EHollowLanguage Language);
}
