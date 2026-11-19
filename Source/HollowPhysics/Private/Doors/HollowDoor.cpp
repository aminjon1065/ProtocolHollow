#include "Doors/HollowDoor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Collision/HollowCollisionChannels.h"
#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowDoor)

AHollowDoor::AHollowDoor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	NetDormancy = DORM_DormantAll;   // server-spawned: sent once, then only on FlushNetDormancy (ADR-0002)

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));
	Leaf->SetupAttachment(Root);
	Leaf->SetCanEverAffectNavigation(false);   // doors reach navigation through links (S2-05), not through tile rebuilds

	Doorway = CreateDefaultSubobject<UBoxComponent>(TEXT("Doorway"));
	Doorway->SetupAttachment(Root);
	Doorway->SetCanEverAffectNavigation(false);
	Doorway->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Doorway->SetCollisionResponseToAllChannels(ECR_Ignore);
	Doorway->SetCollisionResponseToChannel(HollowCollision::Interaction, ECR_Block);
	Doorway->SetGenerateOverlapEvents(false);   // occupancy is an explicit query at decision time (IsDoorwayClear)

	DoorState = CreateDefaultSubobject<UHollowDoorStateComponent>(TEXT("DoorState"));
}

void AHollowDoor::BeginPlay()
{
	Super::BeginPlay();
	LeafClosedLocation = Leaf->GetRelativeLocation();
	LeafMaterial = Leaf->CreateDynamicMaterialInstance(0);
	StateChangedHandle = DoorState->OnStateChanged.AddUObject(this, &ThisClass::HandleStateChanged);
	UpdateLeaf();
	UpdateWeldColor();
}

void AHollowDoor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DoorState->OnStateChanged.Remove(StateChangedHandle);
	GetWorld()->GetTimerManager().ClearTimer(LeafTimer);
	Super::EndPlay(EndPlayReason);
}

bool AHollowDoor::IsDoorwayClear() const
{
	// Asked when it matters (closing on the server), so it never depends on overlap bookkeeping.
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowDoorway), /*bTraceComplex=*/false, this);
	return !GetWorld()->OverlapAnyTestByObjectType(Doorway->GetComponentLocation(), Doorway->GetComponentQuat(),
		FCollisionObjectQueryParams(ECC_Pawn), Doorway->GetCollisionShape(), Params);
}

bool AHollowDoor::CanAcceptToolWork(const FHollowToolWork& Work) const
{
	const TOptional<EHollowDoorEvent> Event = DoorState->ToolEvent(Work.ToolTag);
	return Event.IsSet() && DoorState->Accepts(Event.GetValue());
}

void AHollowDoor::ApplyToolWork(const FHollowToolWork& Work)
{
	if (const TOptional<EHollowDoorEvent> Event = DoorState->ToolEvent(Work.ToolTag))
	{
		DoorState->ApplyTool(Event.GetValue(), Work.Amount);
	}
}

FHollowInteractionInfo AHollowDoor::GetInteractionInfo(const AActor* Interactor) const
{
	FHollowInteractionInfo Info;
	if (const UHollowDoorDefinition* Definition = DoorState->GetDefinition())
	{
		Info.Prompt = DoorState->GetNetState().State == EHollowDoorState::Open ? Definition->PromptClose : Definition->PromptOpen;
	}
	return Info;
}

bool AHollowDoor::CanInteract(const AActor* Interactor) const
{
	// The doorway test belongs to the server's decision; the prompt shows whenever the state allows it.
	return DoorState->Accepts(EHollowDoorEvent::Interact);
}

void AHollowDoor::Interact(AActor* Interactor)
{
	DoorState->Interact(IsDoorwayClear());
}

void AHollowDoor::HandleStateChanged(const FHollowDoorNetState& Old, const FHollowDoorNetState& New, const bool bFresh)
{
	UpdateWeldColor();
	UpdateLeaf();
	const bool bMoving = New.State == EHollowDoorState::Opening || New.State == EHollowDoorState::Closing;
	if (bMoving && LeafUpdateRateHz > 0.f && !GetWorld()->GetTimerManager().IsTimerActive(LeafTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(LeafTimer, this, &ThisClass::UpdateLeaf, 1.f / LeafUpdateRateHz, /*bLoop=*/true);
	}
}

void AHollowDoor::UpdateLeaf()
{
	const UHollowDoorDefinition* Definition = DoorState->GetDefinition();
	if (!Definition)
	{
		return;
	}
	const FHollowDoorNetState& State = DoorState->GetNetState();
	ShownAlpha = HollowDoorRules::LeafAlpha(State, DoorState->GetServerTime(), *Definition);
	Leaf->SetRelativeLocation(LeafClosedLocation + LeafOpenOffset * ShownAlpha);
	Leaf->SetCollisionResponseToChannel(ECC_Pawn, HollowDoorRules::IsLeafBlocking(State.State, ShownAlpha, *Definition) ? ECR_Block : ECR_Ignore);

	// The leaf settles when the motion ends; the server's state change follows a moment later.
	const bool bSettled = (State.State == EHollowDoorState::Opening && ShownAlpha >= 1.f) || (State.State == EHollowDoorState::Closing && ShownAlpha <= 0.f)
		|| (State.State != EHollowDoorState::Opening && State.State != EHollowDoorState::Closing);
	if (bSettled)
	{
		GetWorld()->GetTimerManager().ClearTimer(LeafTimer);
	}
}

void AHollowDoor::UpdateWeldColor()
{
	if (LeafMaterial && !WeldColorParameter.IsNone())
	{
		LeafMaterial->SetVectorParameterValue(WeldColorParameter, FLinearColor::LerpUsingHSV(ClosedColor, WeldedColor, DoorState->GetWeld()));
	}
}
