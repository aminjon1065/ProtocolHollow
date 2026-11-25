#include "FX/HollowFx.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/HollowComfortSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFx)

namespace HollowFxPrivate
{
#if !UE_BUILD_SHIPPING
	int32 GEnabled = 1;
	FAutoConsoleVariableRef CVarEnabled(TEXT("Hollow.Fx.Enabled"), GEnabled,
		TEXT("0: the data's Niagara effects (HollowFx: altar, core, mine) start nowhere on this machine; an A/B capture of their whole cost (FAB-01). Not in Shipping."));
#endif

	/** Nothing plays on a dedicated server, nor with Hollow.Fx.Enabled 0. */
	bool IsDedicatedServer(const UWorld* World)
	{
#if !UE_BUILD_SHIPPING
		if (GEnabled == 0)
		{
			return true;
		}
#endif
		return !World || World->GetNetMode() == NM_DedicatedServer;
	}

	/** The system this player sees, if it is in memory. */
	UNiagaraSystem* Loaded(const FHollowFxSpec& Spec)
	{
		return Cast<UNiagaraSystem>(HollowFx::Chosen(Spec).Get());
	}

	FTransform Local(const FHollowFxSpec& Spec)
	{
		return FTransform(FQuat::Identity, Spec.Offset, FVector(Spec.Scale));
	}

	void Announce(const FHollowFxSpec& Spec, const UNiagaraSystem* System)
	{
#if !UE_BUILD_SHIPPING
		HollowFx::OnPlay().Broadcast(Spec, System);
#endif
	}
}

#if !UE_BUILD_SHIPPING
HollowFx::FOnHollowFxPlay& HollowFx::OnPlay()
{
	static FOnHollowFxPlay Delegate;
	return Delegate;
}
#endif

TSoftObjectPtr<UFXSystemAsset> HollowFx::Chosen(const FHollowFxSpec& Spec)
{
	return IHollowComfortSettings::IsReducedFlashesChosen() ? Spec.ReducedFlashesSystem : Spec.System;
}

TSharedPtr<FStreamableHandle> HollowFx::Preload(const TConstArrayView<const FHollowFxSpec*> Specs, FSimpleDelegate OnLoaded)
{
	TArray<FSoftObjectPath> Paths;
	for (const FHollowFxSpec* Spec : Specs)
	{
		if (!Spec)
		{
			continue;
		}
		for (const TSoftObjectPtr<UFXSystemAsset>* System : {&Spec->System, &Spec->ReducedFlashesSystem})
		{
			if (!System->IsNull())
			{
				Paths.AddUnique(System->ToSoftObjectPath());
			}
		}
	}
	if (Paths.IsEmpty())
	{
		return nullptr;
	}
	return UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths), MoveTemp(OnLoaded));
}

UFXSystemComponent* HollowFx::PlayAttached(const FHollowFxSpec& Spec, USceneComponent* Parent, const FTransform& Frame)
{
	using namespace HollowFxPrivate;
	UNiagaraSystem* System = Parent ? Loaded(Spec) : nullptr;
	if (!System || IsDedicatedServer(Parent->GetWorld()))   // or switched off for an A/B capture
	{
		return nullptr;
	}
	Announce(Spec, System);
	const FTransform Where = Local(Spec) * Frame;
	return UNiagaraFunctionLibrary::SpawnSystemAttached(System, Parent, NAME_None, Where.GetLocation(), Where.Rotator(), Where.GetScale3D(),
		EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true, ENCPoolMethod::None, /*bAutoActivate=*/true);
}

UFXSystemComponent* HollowFx::PlayAt(const FHollowFxSpec& Spec, UWorld* World, const FTransform& Where)
{
	using namespace HollowFxPrivate;
	UNiagaraSystem* System = Loaded(Spec);
	if (!System || IsDedicatedServer(World))
	{
		return nullptr;
	}
	Announce(Spec, System);
	const FTransform Placed = Local(Spec) * Where;
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Placed.GetLocation(), Placed.Rotator(), Placed.GetScale3D(),
		/*bAutoDestroy=*/true, /*bAutoActivate=*/true, ENCPoolMethod::AutoRelease);
	if (Effect && Spec.Seconds > 0.f)
	{
		FTimerHandle StopTimer;
		World->GetTimerManager().SetTimer(StopTimer, FTimerDelegate::CreateWeakLambda(Effect, [Effect] { Effect->Deactivate(); }), Spec.Seconds, /*bLoop=*/false);
	}
	return Effect;
}

void HollowFx::Stop(UFXSystemComponent* Component)
{
	if (Component)
	{
		Component->Deactivate();
	}
}
