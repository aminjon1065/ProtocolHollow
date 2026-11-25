#include "Feel/HollowViewFeelModifier.h"

#include "Camera/PlayerCameraManager.h"
#include "Feel/HollowFeelConfig.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowViewFeelModifier)

void UHollowViewFeelModifier::SetConfig(const UHollowFeelConfig* InConfig)
{
	Config = InConfig;
}

void UHollowViewFeelModifier::AddKick(const FHollowViewKick& Kick, const float Scale)
{
	if (!Kick.IsValid() || Scale <= 0.f)
	{
		return;
	}
	if (Kicks.Num() >= MaxKicks)
	{
		Kicks.RemoveAt(0);
	}
	FKick& Added = Kicks.AddDefaulted_GetRef();
	Added.Params = Kick;
	Added.Scale = Scale;
	Added.YawSign = (KickSerial++ & 1u) ? -1.f : 1.f;   // shot after shot, left and right in turn
}

FRotator UHollowViewFeelModifier::Step(const float DeltaTime, const FRotator& ControlRotation)
{
	float Pitch = 0.f;
	float Yaw = 0.f;
	float PushBack = 0.f;
	for (FKick& Kick : Kicks)
	{
		const float Left = HollowFeelRules::KickLeft(Kick.Params, Kick.Elapsed) * Kick.Scale;
		Pitch += Kick.Params.Pitch * Left;
		Yaw += Kick.Params.Yaw * Kick.YawSign * Left;
		PushBack += Kick.Params.PushBack * Left;
		Kick.Elapsed += FMath::Max(DeltaTime, 0.f);
	}
	Kicks.RemoveAll([](const FKick& Kick) { return Kick.Elapsed >= Kick.Params.Recover; });

	// The hands trail the aim by its turn rate and spring back once it stops.
	FRotator Target = FRotator::ZeroRotator;
	if (Config && bHasLastControl && DeltaTime > UE_KINDA_SMALL_NUMBER)
	{
		const FRotator Turn = (ControlRotation - LastControl).GetNormalized();
		Target.Pitch = FMath::Clamp(-Turn.Pitch / DeltaTime * Config->SwayPerTurnRate, -Config->MaxSway, Config->MaxSway);
		Target.Yaw = FMath::Clamp(-Turn.Yaw / DeltaTime * Config->SwayPerTurnRate, -Config->MaxSway, Config->MaxSway);
	}
	const float Follow = Config && DeltaTime > 0.f ? 1.f - FMath::Exp(-Config->SwayReturnRate * DeltaTime) : 1.f;
	Sway += (Target - Sway) * Follow;
	LastControl = ControlRotation;
	bHasLastControl = true;

	ViewOffset = FRotator(Pitch, Yaw, 0.f);
	HandsPose = FTransform(FRotator(Pitch + Sway.Pitch, Yaw + Sway.Yaw, 0.f), FVector(-PushBack, 0.0, 0.0));
	const bool bOffRest = !HandsPose.Equals(FTransform::Identity, UE_KINDA_SMALL_NUMBER);
	if (bOffRest || bHandsOffRest)
	{
		OnHandsPose.Broadcast(HandsPose);
	}
	bHandsOffRest = bOffRest;
	return ViewOffset;
}

bool UHollowViewFeelModifier::ModifyCamera(const float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	const APlayerController* Player = CameraOwner ? CameraOwner->GetOwningPlayerController() : nullptr;
	InOutPOV.Rotation += Step(DeltaTime, Player ? Player->GetControlRotation() : InOutPOV.Rotation);
	return false;   // the other modifiers (the shakes) still run
}
