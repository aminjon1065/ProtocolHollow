#include "Weapons/HollowWeaponFireCue.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Data/HollowWeaponDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Tools/HollowToolMountComponent.h"
#include "Weapons/HollowGA_WeaponFire.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowWeaponFireCue)

namespace HollowWeaponFireCuePrivate
{
	/** How long each burst spawns (its particles then fade on their own lifetimes). */
	constexpr float FlashSeconds = 0.05f;
	constexpr float TracerSeconds = 0.05f;
	constexpr float ImpactSeconds = 0.1f;

	/** A system of the weapon if it is loaded (the weapon's ability set loads them with the raid; a missing one is skipped). */
	UNiagaraSystem* Loaded(const TSoftObjectPtr<UFXSystemAsset>& System)
	{
		return Cast<UNiagaraSystem>(System.Get());
	}

	/**
	 * A burst of System along Axis, stopped after Seconds: whatever its emitters would do next, it spawns no more and
	 * finishes once its particles die, so Niagara's pool takes it back.
	 */
	void Spawn(UWorld* World, UNiagaraSystem* System, const FVector& Location, const FVector& Axis, const float Seconds)
	{
		UNiagaraComponent* Burst = World && System ? UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location,
			FRotationMatrix::MakeFromZ(Axis).Rotator(), FVector::OneVector, /*bAutoDestroy=*/true, /*bAutoActivate=*/true, ENCPoolMethod::AutoRelease) : nullptr;
		if (Burst)
		{
			FTimerHandle Stop;
			World->GetTimerManager().SetTimer(Stop, FTimerDelegate::CreateWeakLambda(Burst, [Burst] { Burst->Deactivate(); }), Seconds, /*bLoop=*/false);
		}
	}
}

bool UHollowWeaponFireCue::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	using namespace HollowWeaponFireCuePrivate;
	const UHollowWeaponDefinition* Weapon = Cast<UHollowWeaponDefinition>(Parameters.SourceObject.Get());
	APawn* Shooter = Cast<APawn>(Parameters.Instigator.Get());
	UWorld* World = Shooter ? Shooter->GetWorld() : nullptr;
	if (!Weapon || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	FVector Muzzle = Parameters.Location;
	if (const UHollowToolMountComponent* Mount = UHollowToolMountComponent::Get(Shooter))
	{
		Mount->GetMuzzleLocation(Muzzle);   // the held weapon's, as this machine sees it
	}
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(Shooter); Audio && Weapon->NoiseTag.IsValid())
	{
		Audio->Play(Weapon->NoiseTag, Muzzle);
	}
	const FVector Aim = FVector(Parameters.Normal).GetSafeNormal();
	Spawn(World, Loaded(Weapon->MuzzleFlash), Muzzle, Aim, FlashSeconds);

	UNiagaraSystem* Tracer = Loaded(Weapon->Tracer);
	UNiagaraSystem* Impact = Loaded(Weapon->Impact);
	if (!Tracer && !Impact)
	{
		return true;
	}
	TArray<FHitResult> Pellets;
	UHollowGA_WeaponFire::TracePellets(*World, *Weapon, Shooter, Parameters.Location, Aim, static_cast<uint16>(Parameters.RawMagnitude),
		Parameters.NormalizedMagnitude > 0.5f, Pellets);
	for (const FHitResult& Pellet : Pellets)
	{
		const FVector End = Pellet.bBlockingHit ? Pellet.ImpactPoint : Pellet.TraceEnd;
		Spawn(World, Tracer, Muzzle, (End - Muzzle).GetSafeNormal(), TracerSeconds);
		if (Pellet.bBlockingHit)
		{
			Spawn(World, Impact, Pellet.ImpactPoint, Pellet.ImpactNormal, ImpactSeconds);
		}
	}
	return true;
}
