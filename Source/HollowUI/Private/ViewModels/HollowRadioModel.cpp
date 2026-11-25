#include "ViewModels/HollowRadioModel.h"

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Assembly/HollowSectorStateComponent.h"
#include "Audio/HollowAudioSubsystem.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarryableComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/HollowEnemyCharacter.h"
#include "Enemies/HollowStalkerDefinition.h"
#include "Foundation/HollowUISettings.h"
#include "GameFramework/PlayerController.h"
#include "HollowUILog.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Raid/HollowRaidStateComponent.h"
#include "Tags/HollowGameplayTags.h"
#include "TimerManager.h"
#include "ViewModels/HollowObjectiveModel.h"
#include "ViewModels/HollowVisorContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowRadioModel)

namespace HollowRadioModelPrivate
{
	/** A timer never waits less than this (a line that ends now). */
	constexpr float MinWakeSeconds = 0.01f;

	FString CueName(const FHollowRadioQueue& Queue, const int32 Cue)
	{
		const UHollowRadioScript* Script = Queue.GetScript();
		return Script && Script->Cues.IsValidIndex(Cue) ? Script->Cues[Cue].Id.ToString() : FString(TEXT("?"));
	}
}

void UHollowRadioModel::Initialize(UHollowVisorContext& InContext)
{
	Context = &InContext;
	if (!Script)
	{
		Script = GetDefault<UHollowUISettings>()->RadioScript.LoadSynchronous();
	}
	Queue.SetScript(Script);
	SourcesHandle = InContext.OnSourcesChanged.AddUObject(this, &ThisClass::BindSources);
	if (UHollowObjectiveModel* Objective = InContext.GetObjective())
	{
		ObjectiveHandle = Objective->OnObjectiveChanged.AddUObject(this, &ThisClass::Refresh);   // a core lifted, a ritual, a death
	}
	BindSources();
}

void UHollowRadioModel::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WakeTimer);
		World->GetTimerManager().ClearTimer(SensorTimer);
	}
	if (UHollowVisorContext* Owner = Context.Get())
	{
		Owner->OnSourcesChanged.Remove(SourcesHandle);
		if (UHollowObjectiveModel* Objective = Owner->GetObjective())
		{
			Objective->OnObjectiveChanged.Remove(ObjectiveHandle);
		}
	}
	if (UHollowRaidStateComponent* Raid = RaidState.Get())
	{
		Raid->OnStateChanged.Remove(RaidHandle);
		Raid->OnLoreFound.Remove(LoreHandle);
	}
	Tier.Unbind();
	BoundAbilities.Reset();
	Queue.Clear();
	RaidState.Reset();
	Context.Reset();
}

UWorld* UHollowRadioModel::GetWorld() const
{
	const UHollowVisorContext* Owner = Context.Get();
	return Owner ? Owner->GetWorld() : nullptr;
}

const UHollowRadioScript* UHollowRadioModel::GetScript() const
{
	return Script;
}

void UHollowRadioModel::SetScriptForTests(const UHollowRadioScript* InScript)
{
	Script = InScript;
	Queue.SetScript(InScript);
	UpdateSubtitle();
}

void UHollowRadioModel::SetHeardForTests(TSet<FName>* InHeard)
{
	TestHeard = InHeard;
}

void UHollowRadioModel::SetClockForTests(TFunction<double()> InClock)
{
	TestClock = MoveTemp(InClock);
}

double UHollowRadioModel::Now() const
{
	if (TestClock)
	{
		return TestClock();
	}
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void UHollowRadioModel::BindSources()
{
	UHollowRaidStateComponent* NewRaid = UHollowRaidStateComponent::Get(GetWorld());
	if (RaidState.Get() != NewRaid)
	{
		if (UHollowRaidStateComponent* Old = RaidState.Get())
		{
			Old->OnStateChanged.Remove(RaidHandle);
			Old->OnLoreFound.Remove(LoreHandle);
		}
		RaidState = NewRaid;
		if (NewRaid)
		{
			RaidHandle = NewRaid->OnStateChanged.AddUObject(this, &ThisClass::HandleRaidState);
			LoreHandle = NewRaid->OnLoreFound.AddUObject(this, &ThisClass::HandleLoreFound);
		}
	}
	UHollowAbilitySystemComponent* Abilities = Context.IsValid() ? Context->GetAbilitySystem() : nullptr;
	if (BoundAbilities.Get() != Abilities)
	{
		BoundAbilities = Abilities;
		if (Abilities)
		{
			Tier.Bind(Abilities, [this](EHollowResonanceTier) { Refresh(); });   // calls back at once
			return;
		}
		Tier.Unbind();
	}
	Refresh();
}

void UHollowRadioModel::HandleRaidState(const FHollowRaidNetState& OldState, const FHollowRaidNetState& NewState)
{
	Refresh();
}

void UHollowRadioModel::HandleLoreFound(const FPrimaryAssetId& Entry, const bool bNew)
{
	const UHollowRaidStateComponent* Raid = RaidState.Get();
	const FGuid RaidId = Raid ? Raid->GetState().RaidId : FGuid();
	if (LoreRaid != RaidId)
	{
		LoreRaid = RaidId;
		NewLore = 0;
	}
	NewLore += bNew ? 1 : 0;
	Refresh();
}

void UHollowRadioModel::GatherFacts(FHollowRadioFacts& OutFacts) const
{
	const UWorld* World = GetWorld();
	const UHollowVisorContext* Owner = Context.Get();
	const UHollowObjectiveModel* Objective = Owner ? Owner->GetObjective() : nullptr;
	const FHollowObjectiveFacts* Known = Objective ? &Objective->GetFacts() : nullptr;
	const UHollowRaidStateComponent* Raid = RaidState.Get();
	const FHollowRaidNetState* Net = Raid ? &Raid->GetState() : nullptr;

	OutFacts = FHollowRadioFacts();
	OutFacts.bInRaid = Net && Net->RaidId.IsValid();
	OutFacts.bInHub = !OutFacts.bInRaid && Known && Known->bInHub;
	OutFacts.RaidId = OutFacts.bInRaid ? Net->RaidId : FGuid();
	OutFacts.Phase = Net ? Net->Phase : EHollowRaidPhase::Assembling;
	OutFacts.LivingPlayers = Net ? Net->LivingPlayers : 0;
	if (const UHollowSectorStateComponent* Sector = OutFacts.bInRaid ? UHollowSectorStateComponent::Get(World) : nullptr)
	{
		OutFacts.Sector = Sector->GetManifest().SectorPlugin;
	}
	OutFacts.bHasBody = Owner && Owner->GetCharacter();
	OutFacts.bAlive = OutFacts.bHasBody && Known && Known->bAlive;
	OutFacts.bCoreLifted = OutFacts.bInRaid && Known && Known->bCoreLifted;
	OutFacts.bRitual = OutFacts.bInRaid && Known && Known->bRitual;
	const bool bSensedHere = OutFacts.bInRaid && SensedRaid == OutFacts.RaidId;
	OutFacts.bCoreSighted = bSensedHere && bCoreSighted;
	OutFacts.bStalkerSighted = bSensedHere && bStalkerSighted;
	OutFacts.Tier = Tier.IsBound() ? Tier.GetTier() : EHollowResonanceTier::Low;
	OutFacts.NewLore = OutFacts.bInRaid && LoreRaid == OutFacts.RaidId ? NewLore : 0;
}

void UHollowRadioModel::Refresh()
{
	if (!Context.IsValid())
	{
		return;
	}
	FHollowRadioFacts New;
	GatherFacts(New);
	const TArray<FGameplayTag> Triggers = HollowRadioRules::TriggersFor(Facts, New);
	Facts = New;
	bool bQueued = false;
	for (const FGameplayTag& Trigger : Triggers)
	{
		const int32 Cue = Queue.Fire(Trigger, Facts.Sector, Facts.RaidId, Now(), [this](const FName Id) { return HasHeard(Id); });
		UE_LOG(LogHollowUI, Log, TEXT("Radio of %s: %s -> %s"), *GetNameSafe(Context->GetPlayer()), *Trigger.ToString(),
			Cue == INDEX_NONE ? TEXT("nothing to say") : *FString::Printf(TEXT("cue %s queued"), *HollowRadioModelPrivate::CueName(Queue, Cue)));
		bQueued |= Cue != INDEX_NONE;
	}
	UpdateSensorTimer();
	if (bQueued)
	{
		Update();
	}
}

bool UHollowRadioModel::Fire(const FGameplayTag& Trigger, const bool bForce)
{
	const int32 Cue = Queue.Fire(Trigger, Facts.Sector, Facts.RaidId, Now(), [this](const FName Id) { return HasHeard(Id); }, bForce);
	UE_LOG(LogHollowUI, Log, TEXT("Radio of %s: %s%s -> %s"), *GetNameSafe(Context.IsValid() ? Context->GetPlayer() : nullptr), *Trigger.ToString(),
		bForce ? TEXT(" (forced)") : TEXT(""), Cue == INDEX_NONE ? TEXT("nothing to say") : *FString::Printf(TEXT("cue %s queued"), *HollowRadioModelPrivate::CueName(Queue, Cue)));
	Update();
	return Cue != INDEX_NONE;
}

void UHollowRadioModel::Forget()
{
	Queue.Forget();
	bCoreSighted = false;
	bStalkerSighted = false;
	SensedRaid.Invalidate();
	Refresh();
}

void UHollowRadioModel::Update()
{
	ApplyEvents(Queue.Advance(Now()));
	Schedule();
}

void UHollowRadioModel::ApplyEvents(const FHollowRadioEvents& Events)
{
	using namespace HollowRadioModelPrivate;
	if (Events.bClosed)
	{
		PlayClick(HollowTags::Sound_Radio_Close);
	}
	if (Events.bOpened)
	{
		PlayClick(HollowTags::Sound_Radio_Open);
	}
	const UHollowRadioScript* Lines = Script;
	for (const int32 Cue : Events.Finished)
	{
		const bool bForGood = Lines && Lines->Cues.IsValidIndex(Cue) && Lines->Cues[Cue].Scope == EHollowRadioScope::Profile;
		if (bForGood)
		{
			MarkHeard(Lines->Cues[Cue].Id);
		}
		UE_LOG(LogHollowUI, Log, TEXT("Radio: cue %s heard%s"), *CueName(Queue, Cue), bForGood ? TEXT(" (remembered in the profile)") : TEXT(""));
	}
	for (const int32 Cue : Events.Dropped)
	{
		UE_LOG(LogHollowUI, Log, TEXT("Radio: cue %s dropped (waited too long or pushed out)"), *CueName(Queue, Cue));
	}
	if (Events.bLineChanged)
	{
		UpdateSubtitle();
		OnRadioChanged.Broadcast();
	}
}

void UHollowRadioModel::Schedule()
{
	UWorld* World = GetWorld();
	if (!World || TestClock)
	{
		return;   // tests move the clock themselves
	}
	FTimerManager& Timers = World->GetTimerManager();
	Timers.ClearTimer(WakeTimer);
	if (const TOptional<double> Wake = Queue.GetNextWake())
	{
		const float Delay = FMath::Max(static_cast<float>(Wake.GetValue() - Now()), HollowRadioModelPrivate::MinWakeSeconds);
		Timers.SetTimer(WakeTimer, this, &ThisClass::Update, Delay, /*bLoop=*/false);
	}
}

void UHollowRadioModel::UpdateSubtitle()
{
	const FHollowRadioLine* Line = Queue.GetPlayingLine();
	Subtitle = FHollowRadioSubtitle();
	if (Line && Script)
	{
		Subtitle.bShown = true;
		Subtitle.Speaker = Line->Speaker;
		Subtitle.SpeakerName = Script->GetSpeakerName(Line->Speaker);
		Subtitle.Text = Line->Text;
	}
}

void UHollowRadioModel::UpdateSensorTimer()
{
	UWorld* World = GetWorld();
	if (!World || !Script)
	{
		return;
	}
	FTimerManager& Timers = World->GetTimerManager();
	const bool bSense = HollowRadioRules::ShouldSense(Facts);
	if (bSense && !Timers.IsTimerActive(SensorTimer))
	{
		Timers.SetTimer(SensorTimer, this, &ThisClass::Sense, Script->SensorIntervalSeconds, /*bLoop=*/true);
	}
	else if (!bSense)
	{
		Timers.ClearTimer(SensorTimer);
	}
}

bool UHollowRadioModel::HasLineOfSight(const FVector& From, const AActor& Target) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowRadioSight), /*bTraceComplex=*/false);
	if (const UHollowVisorContext* Owner = Context.Get())
	{
		Params.AddIgnoredActor(Owner->GetPlayer() ? Owner->GetPlayer()->GetPawn() : nullptr);
	}
	FHitResult Hit;
	return !World->LineTraceSingleByChannel(Hit, From, Target.GetActorLocation(), ECC_Visibility, Params) || Hit.GetActor() == &Target;
}

void UHollowRadioModel::Sense()
{
	UWorld* World = GetWorld();
	const APlayerController* Player = Context.IsValid() ? Context->GetPlayer() : nullptr;
	if (!World || !Player || !Script || !HollowRadioRules::ShouldSense(Facts))
	{
		UpdateSensorTimer();
		return;
	}
	if (SensedRaid != Facts.RaidId)
	{
		SensedRaid = Facts.RaidId;
		bCoreSighted = false;
		bStalkerSighted = false;
	}
	FVector Eye;
	FRotator View;
	Player->GetPlayerViewPoint(Eye, View);
	const FVector Direction = View.Vector();
	const float HalfAngle = Script->SightHalfAngleDegrees;
	if (!bCoreSighted && !Facts.bCoreLifted)
	{
		for (TActorIterator<AHollowAetherCore> It(World); It; ++It)
		{
			const bool bResting = !It->GetCarryable()->GetCarryState().IsCarried();
			if (bResting && HollowRadioRules::IsInSight(Eye, Direction, It->GetActorLocation(), Script->CoreSightDistance, HalfAngle) && HasLineOfSight(Eye, **It))
			{
				bCoreSighted = true;
				break;
			}
		}
	}
	if (!bStalkerSighted)
	{
		for (TActorIterator<AHollowEnemyCharacter> It(World); It; ++It)
		{
			const UHollowEnemyDefinition* Definition = It->GetDefinition();
			const bool bStalker = Definition && Definition->IsA<UHollowStalkerDefinition>() && !It->IsDead() && !It->IsInVent();
			if (bStalker && HollowRadioRules::IsInSight(Eye, Direction, It->GetActorLocation(), Script->StalkerSightDistance, HalfAngle) && HasLineOfSight(Eye, **It))
			{
				bStalkerSighted = true;
				break;
			}
		}
	}
	Refresh();
}

void UHollowRadioModel::PlayClick(const FGameplayTag& Sound) const
{
	const APlayerController* Player = Context.IsValid() ? Context->GetPlayer() : nullptr;
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(GetWorld());
	if (!Audio)
	{
		return;
	}
	FVector Eye = FVector::ZeroVector;
	FRotator View;
	if (Player)
	{
		Player->GetPlayerViewPoint(Eye, View);   // in the listener's helmet
	}
	Audio->Play(Sound, Eye);
}

bool UHollowRadioModel::HasHeard(const FName CueId) const
{
	if (TestHeard)
	{
		return TestHeard->Contains(CueId);
	}
	const UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this);
	return Profile && Profile->HasHeardRadio(CueId);
}

void UHollowRadioModel::MarkHeard(const FName CueId)
{
	if (TestHeard)
	{
		TestHeard->Add(CueId);
	}
	else if (UHollowProfileSubsystem* Profile = UHollowProfileSubsystem::Get(this))
	{
		Profile->MarkRadioHeard(CueId);
	}
}

FString UHollowRadioModel::Describe() const
{
	using namespace HollowRadioModelPrivate;
	const FHollowRadioPlaying* Playing = Queue.GetPlaying();
	const FHollowRadioCue* Cue = Queue.GetPlayingCue();
	if (!Playing || !Cue || !Subtitle.bShown)
	{
		return FString::Printf(TEXT("radio quiet, %d waiting"), Queue.GetNumQueued());
	}
	return FString::Printf(TEXT("radio on the air: cue %s line %d/%d %s \"%s\", %d waiting"), *Cue->Id.ToString(), Playing->Line + 1,
		Cue->Variants.IsValidIndex(Playing->Variant) ? Cue->Variants[Playing->Variant].Lines.Num() : 0, *Subtitle.SpeakerName.ToString(),
		*Subtitle.Text.ToString(), Queue.GetNumQueued());
}
