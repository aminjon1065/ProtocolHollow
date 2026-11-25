#include "Feel/HollowViewFeelComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/HollowHealthSet.h"
#include "Feel/HollowFeelSubsystem.h"
#include "Feel/HollowViewFeelModifier.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Tags/HollowGameplayTags.h"
#include "Tools/HollowToolMountComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowViewFeelComponent)

UHollowViewFeelComponent::UHollowViewFeelComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);   // local presentation only
}

UHollowViewFeelComponent* UHollowViewFeelComponent::Get(const AActor* Owner)
{
	return Owner ? Owner->FindComponentByClass<UHollowViewFeelComponent>() : nullptr;
}

void UHollowViewFeelComponent::Refresh()
{
	Unbind();
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UHollowFeelSubsystem* Feel = UHollowFeelSubsystem::Get(this);
	if (!Player || !Player->IsLocalController() || !Feel)
	{
		return;   // someone else's body, or the server's copy of it: nothing to feel here
	}
	if (UHollowViewFeelModifier* View = Feel->GetViewModifier(*Player))
	{
		BoundView = View;
		PoseHandle = View->OnHandsPose.AddWeakLambda(this, [this](const FTransform& Pose)
		{
			if (UHollowToolMountComponent* Mount = UHollowToolMountComponent::Get(GetOwner()))
			{
				Mount->SetViewFeelOffset(Pose);
			}
		});
	}
	if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
	{
		BoundAbilitySystem = AbilitySystem;
		HealthHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UHollowHealthSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealthChanged);
	}
}

void UHollowViewFeelComponent::Unbind()
{
	if (UHollowViewFeelModifier* View = BoundView.Get())
	{
		View->OnHandsPose.Remove(PoseHandle);
	}
	if (UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(UHollowHealthSet::GetHealthAttribute()).Remove(HealthHandle);
	}
	if (UHollowToolMountComponent* Mount = UHollowToolMountComponent::Get(GetOwner()))
	{
		Mount->SetViewFeelOffset(FTransform::Identity);
	}
	BoundView.Reset();
	BoundAbilitySystem.Reset();
	PoseHandle.Reset();
	HealthHandle.Reset();
}

void UHollowViewFeelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Unbind();
	Super::EndPlay(EndPlayReason);
}

float UHollowViewFeelComponent::DamageFeelScale(const float Lost, const float MaxHealth) const
{
	return Lost > 0.f && MaxHealth > 0.f ? FMath::Min(Lost / MaxHealth / FullJoltShare, 1.f) : 0.f;
}

void UHollowViewFeelComponent::HandleHealthChanged(const FOnAttributeChangeData& Change)
{
	const UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get();
	UHollowFeelSubsystem* Feel = UHollowFeelSubsystem::Get(this);
	if (!AbilitySystem || !Feel || Change.NewValue >= Change.OldValue)
	{
		return;
	}
	const float Scale = DamageFeelScale(Change.OldValue - Change.NewValue, AbilitySystem->GetNumericAttribute(UHollowHealthSet::GetMaxHealthAttribute()));
	if (Scale > 0.f)
	{
		Feel->EmitFor(HollowTags::Feel_Damage, GetOwner(), Scale);
	}
}
