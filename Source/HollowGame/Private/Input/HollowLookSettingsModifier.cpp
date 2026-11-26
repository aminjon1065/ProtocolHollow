#include "Input/HollowLookSettingsModifier.h"

#include "Settings/HollowGameUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLookSettingsModifier)

FVector2D UHollowLookSettingsModifier::Apply(const FVector2D& Look, const float Sensitivity, const bool bInverted)
{
	return FVector2D(Look.X * Sensitivity, (bInverted ? -Look.Y : Look.Y) * Sensitivity);
}

FInputActionValue UHollowLookSettingsModifier::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, const FInputActionValue CurrentValue, const float DeltaTime)
{
	const UHollowGameUserSettings* Settings = UHollowGameUserSettings::Get();
	if (!Settings || CurrentValue.GetValueType() != EInputActionValueType::Axis2D)
	{
		return CurrentValue;
	}
	const float Sensitivity = bApplySensitivity ? Settings->GetMouseSensitivity() : 1.f;
	return FInputActionValue(Apply(CurrentValue.Get<FVector2D>(), Sensitivity, Settings->IsLookInverted()));
}
