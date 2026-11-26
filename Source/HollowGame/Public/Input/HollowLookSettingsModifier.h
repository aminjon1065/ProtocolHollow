#pragma once

#include "InputModifiers.h"

#include "HollowLookSettingsModifier.generated.h"

/**
 * UI-02 (ADR-0052): the player's look settings on a look mapping of the input data (IMC_ExoFrame, IA_Look): the mouse
 * sensitivity where bApplySensitivity (the mouse's mapping; the gamepad keeps the data's speed) and the inverted look
 * on every mapping. Reads UHollowGameUserSettings on each input, so a change applies at once; no state of its own.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Hollow Look Settings"))
class HOLLOWGAME_API UHollowLookSettingsModifier : public UInputModifier
{
	GENERATED_BODY()

public:
	/** The look input after the settings: X and Y scaled by Sensitivity, Y negated when inverted. */
	static FVector2D Apply(const FVector2D& Look, float Sensitivity, bool bInverted);

	/** The mouse's mapping: the sensitivity applies (the gamepad's mapping applies only the inversion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bApplySensitivity = true;

protected:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};
