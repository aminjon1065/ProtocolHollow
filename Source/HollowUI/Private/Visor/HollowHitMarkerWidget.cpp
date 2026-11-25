#include "Visor/HollowHitMarkerWidget.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "ViewModels/HollowVisorContext.h"
#include "Visor/HollowVisorStyle.h"
#include "Weapons/HollowHandsComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowHitMarkerWidget)

UHollowHitMarkerWidget::UHollowHitMarkerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);   // seen, never clicked
}

void UHollowHitMarkerWidget::BindContext(UHollowVisorContext& InContext)
{
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::Rebind);
	Rebind();
}

void UHollowHitMarkerWidget::UnbindContext(UHollowVisorContext& InContext)
{
	InContext.OnSourcesChanged.Remove(SourcesHandle);
	Unbind();
	Hide();
}

void UHollowHitMarkerWidget::Unbind()
{
	if (UHollowHandsComponent* Old = Hands.Get())
	{
		Old->OnHitMarker.Remove(HandsHandle);
	}
	Hands.Reset();
}

void UHollowHitMarkerWidget::Rebind()
{
	Unbind();
	const UHollowVisorContext* Context = GetContext();
	if (UHollowHandsComponent* NewHands = UHollowHandsComponent::Get(Context ? Context->GetCharacter() : nullptr))
	{
		HandsHandle = NewHands->OnHitMarker.AddUObject(this, &ThisClass::Show);
		Hands = NewHands;
	}
	Hide();
}

void UHollowHitMarkerWidget::Show(const EHollowHitKind Kind)
{
	const UHollowVisorStyle* Style = GetContext() ? GetContext()->GetStyle() : nullptr;
	UWorld* World = GetWorld();
	if (Kind == EHollowHitKind::None || !Style || !World)
	{
		return;
	}
	Shown = Kind;
	bShowing = true;
	++ShownCount;
	const bool bArmor = Kind == EHollowHitKind::Armor;
	const bool bCrit = Kind == EHollowHitKind::Crit;
	if (Marker)
	{
		Marker->SetColorAndOpacity(bCrit ? Style->CritColor : (bArmor ? Style->ArmorColor : Style->HitColor));
		const float Scale = bCrit ? Style->CritMarkerScale : (bArmor ? Style->ArmorMarkerScale : 1.f);
		Marker->SetRenderScale(FVector2D(Scale, Scale));
		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(World))
	{
		const AActor* Body = GetContext()->GetCharacter();
		const FGameplayTag Sound = bCrit ? HollowTags::Sound_UI_HitMarker_Crit : (bArmor ? HollowTags::Sound_UI_HitMarker_Armor : HollowTags::Sound_UI_HitMarker_Hit);
		Audio->Play(Sound, Body ? Body->GetActorLocation() : FVector::ZeroVector);   // in the helmet: at the player
	}
	World->GetTimerManager().SetTimer(HideTimer, FTimerDelegate::CreateUObject(this, &ThisClass::Hide), FMath::Max(Style->HitMarkerSeconds, UE_KINDA_SMALL_NUMBER),
		/*bLoop=*/false);
}

void UHollowHitMarkerWidget::Hide()
{
	bShowing = false;
	if (Marker)
	{
		Marker->SetVisibility(ESlateVisibility::Hidden);
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}
}

FString UHollowHitMarkerWidget::Describe() const
{
	return FString::Printf(TEXT("hit marker %s, %d shown"), bShowing ? LexToString(Shown) : TEXT("hidden"), ShownCount);
}
