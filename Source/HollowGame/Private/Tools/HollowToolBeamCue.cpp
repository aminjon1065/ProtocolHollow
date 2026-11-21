#include "Tools/HollowToolBeamCue.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HollowToolDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HollowGameLog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Tools/HollowToolAim.h"
#include "Tools/HollowToolMountComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowToolBeamCue)

AHollowToolBeamCue::AHollowToolBeamCue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bAutoDestroyOnRemove = true;

	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);
	Beam->SetVisibility(false);
	SetRootComponent(Beam);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Beam);
	Glow->SetUsingAbsoluteLocation(true);
	Glow->SetUsingAbsoluteScale(true);
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);
}

bool AHollowToolBeamCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	Wielder = Cast<APawn>(MyTarget);
	Tool = Cast<UHollowToolDefinition>(Parameters.SourceObject.Get());
	if (!Wielder.IsValid() || !Tool.IsValid() || UpdateRateHz <= 0.f)
	{
		UE_LOG(LogHollowGame, Warning, TEXT("%s: beam needs a pawn, a tool definition and UpdateRateHz."), *GetName());
		return false;
	}

	BeamMaterial = Beam->CreateDynamicMaterialInstance(0);
	if (BeamMaterial && !ColorParameter.IsNone())
	{
		BeamMaterial->SetVectorParameterValue(ColorParameter, Tool->BeamColor);
	}
	Glow->SetLightColor(Tool->BeamColor);
	if (UHollowToolMountComponent* Mount = UHollowToolMountComponent::Get(Wielder.Get()))
	{
		Mount->ShowTool(Tool.Get());   // the running tool in hand on this machine too (cosmetic; the cue is what replicates)
	}
	Beam->SetVisibility(true);
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
	{
		Sound = Audio->PlayAttached(Tool->NoiseTag, Wielder->GetRootComponent());
	}
	UpdateBeam();
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateBeam, 1.f / UpdateRateHz, /*bLoop=*/true);
	UE_LOG(LogHollowGame, Log, TEXT("Tool beam on: %s (%s, %s)"), *Wielder->GetName(), *Tool->ToolTag.ToString(),
		Wielder->IsLocallyControlled() ? TEXT("own") : TEXT("remote"));
	return true;
}

bool AHollowToolBeamCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	Beam->SetVisibility(false);
	Glow->SetVisibility(false);
	if (Sound)
	{
		Sound->Stop();
		Sound = nullptr;
	}
	return Super::OnRemove_Implementation(MyTarget, Parameters);
}

void AHollowToolBeamCue::UpdateBeam()
{
	const APawn* Pawn = Wielder.Get();
	const UHollowToolDefinition* Definition = Tool.Get();
	const UStaticMesh* Mesh = Beam->GetStaticMesh();
	if (!Pawn || !Definition || !Mesh)
	{
		return;
	}

	// Own trace on this machine: owners use their control rotation, remote wielders their replicated aim.
	const FHollowToolAimTrace Trace = HollowToolAim::TraceLocal(*Pawn, *Definition);
	const bool bHit = Trace.bHit;
	const FVector Impact = Trace.End;
	const FVector Muzzle = HollowToolAim::BeamStart(*Pawn, Trace, MuzzleOffset);   // the held tool's muzzle, or in front of the eyes

	const FVector Span = Impact - Muzzle;
	const FVector MeshSize = Mesh->GetBoundingBox().GetSize();
	if (Span.IsNearlyZero() || MeshSize.Z <= UE_KINDA_SMALL_NUMBER || MeshSize.X <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Scale(Definition->BeamWidth / MeshSize.X, Definition->BeamWidth / MeshSize.X, Span.Size() / MeshSize.Z);
	SetActorTransform(FTransform(FRotationMatrix::MakeFromZ(Span).Rotator(), Muzzle + Span * 0.5, Scale));
	Glow->SetWorldLocation(Impact);
	Glow->SetVisibility(bHit);
}
