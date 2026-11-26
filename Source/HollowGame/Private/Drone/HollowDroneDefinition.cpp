#include "Drone/HollowDroneDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDroneDefinition)

#define LOCTEXT_NAMESPACE "HollowDroneDefinition"

#if WITH_EDITOR
EDataValidationResult UHollowDroneDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Fail = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (FollowOffset.X <= 0.0 || FollowOffset.Size() <= ProbeRadius)
	{
		Fail(LOCTEXT("Follow", "A drone hovers in front of its player (FollowOffset.X > 0), further away than its probe is wide."));
	}
	if (HeadingDeadZone <= FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(FollowOffset.Y), FMath::Max(FollowOffset.X, UE_KINDA_SMALL_NUMBER))))
	{
		Fail(LOCTEXT("DeadZone", "HeadingDeadZone must be wider than the hover's angle beside the view, or turning to face the drone chases it away."));
	}
	if (CarryPointOffset.Z >= 0.0)
	{
		Fail(LOCTEXT("CarryPoint", "A drone carries its load below it (CarryPointOffset.Z < 0)."));
	}
	if (HandOverPrompt.IsEmpty() || TakeBackPrompt.IsEmpty())
	{
		Fail(LOCTEXT("Prompts", "A drone needs both prompts (hand over, take back)."));
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
