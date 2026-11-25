#include "Feel/HollowFeelSubsystem.h"

#include "Audio/HollowAudioSubsystem.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Feel/HollowFeelCameraShake.h"
#include "Feel/HollowFeelConfig.h"
#include "Feel/HollowViewFeelModifier.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HollowCoreLog.h"
#include "Interfaces/HollowComfortSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFeelSubsystem)

UHollowFeelSubsystem* UHollowFeelSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UHollowFeelSubsystem>() : nullptr;
}

bool UHollowFeelSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);   // nobody looks through a dedicated server
}

bool UHollowFeelSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowFeelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SetConfig(UHollowFeelConfig::Get());
	if (UHollowAudioSubsystem* Audio = Collection.InitializeDependency<UHollowAudioSubsystem>())
	{
		SoundHandle = Audio->OnSoundPlayed.AddUObject(this, &ThisClass::HandleSoundPlayed);
	}
}

void UHollowFeelSubsystem::Deinitialize()
{
	if (UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(this))
	{
		Audio->OnSoundPlayed.Remove(SoundHandle);
	}
	Super::Deinitialize();
}

void UHollowFeelSubsystem::SetConfig(const UHollowFeelConfig* InConfig)
{
	Config = InConfig;
}

float UHollowFeelSubsystem::GetComfortScale() const
{
	return Config && IHollowComfortSettings::IsReducedFlashesChosen() ? Config->ReducedFlashesScale : 1.f;
}

bool UHollowFeelSubsystem::EmitFor(const FGameplayTag& Trigger, const AActor* Viewer, const float Scale)
{
	const FHollowFeelEntry* Entry = Config ? Config->Find(Trigger) : nullptr;
	const APawn* Pawn = Cast<APawn>(Viewer);
	APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Entry || !Player || !Player->IsLocalController() || Scale <= 0.f)
	{
		return false;
	}
	Feel(*Player, *Entry, Scale);
	return true;
}

int32 UHollowFeelSubsystem::EmitAt(const FGameplayTag& Trigger, const FVector& Location, const float Scale)
{
	const FHollowFeelEntry* Entry = Config ? Config->Find(Trigger) : nullptr;
	UWorld* World = GetWorld();
	if (!Entry || !World || Entry->IsPersonal() || Scale <= 0.f)
	{
		return 0;
	}
	int32 Felt = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (!Player || !Player->IsLocalController())
		{
			continue;
		}
		// The body feels it (the camera of a player that died looks on from elsewhere: no body, no feel).
		const APawn* Pawn = Player->GetPawn();
		const float Reach = Pawn ? HollowFeelRules::ScaleAt(*Entry, FVector::Dist(Pawn->GetActorLocation(), Location)) : 0.f;
		if (Reach > 0.f)
		{
			Feel(*Player, *Entry, Reach * Scale);
			++Felt;
		}
	}
	return Felt;
}

void UHollowFeelSubsystem::HandleSoundPlayed(const FGameplayTag& SoundTag, const FVector& Location, const float VolumeScale)
{
	EmitAt(SoundTag, Location, FMath::Min(VolumeScale, 1.f));   // a slow step is a soft one; louder than full is still full
}

void UHollowFeelSubsystem::Feel(APlayerController& Player, const FHollowFeelEntry& Entry, const float Scale)
{
	const float Comfort = GetComfortScale();
	Counts.FindOrAdd(Entry.Trigger) += 1;
	LastScale = Scale * Comfort;
	if (Entry.Shake.IsValid() && Player.PlayerCameraManager && LastScale > 0.f)
	{
		FAddCameraShakeParams Params(LastScale);
		const FHollowShakeParams Shake = Entry.Shake;
		const uint32 Seed = ShakeSerial++;
		Params.Initializer.BindLambda([Shake, Seed](UCameraShakeBase* Instance)
		{
			if (UHollowFeelCameraShake* FeelShake = Cast<UHollowFeelCameraShake>(Instance))
			{
				FeelShake->Setup(Shake, Seed);
			}
		});
		Player.PlayerCameraManager->StartCameraShake(UHollowFeelCameraShake::StaticClass(), Params);
	}
	if (Entry.Rumble.IsValid())
	{
		// Rumble is not a flash: it keeps its strength with reduced flashes.
		const bool bHeavy = Entry.Rumble.bHeavy;
		Player.PlayDynamicForceFeedback(FMath::Min(Entry.Rumble.Intensity * Scale, 1.f), Entry.Rumble.Duration, bHeavy, !bHeavy, bHeavy, !bHeavy);
	}
	if (Entry.Kick.IsValid())
	{
		if (UHollowViewFeelModifier* View = GetViewModifier(Player))
		{
			View->AddKick(Entry.Kick, LastScale);
		}
	}
	UE_LOG(LogHollowCore, Verbose, TEXT("Feel %s for %s at %.2f"), *Entry.Trigger.ToString(), *Player.GetName(), LastScale);
}

UHollowViewFeelModifier* UHollowFeelSubsystem::GetViewModifier(APlayerController& Player)
{
	APlayerCameraManager* Camera = Player.PlayerCameraManager;
	if (!Camera)
	{
		return nullptr;
	}
	UHollowViewFeelModifier* View = Cast<UHollowViewFeelModifier>(Camera->FindCameraModifierByClass(UHollowViewFeelModifier::StaticClass()));
	if (!View)
	{
		View = Cast<UHollowViewFeelModifier>(Camera->AddNewCameraModifier(UHollowViewFeelModifier::StaticClass()));
		if (View)
		{
			View->SetConfig(Config);
		}
	}
	return View;
}

int32 UHollowFeelSubsystem::GetFeltCount(const FGameplayTag& Trigger) const
{
	const int32* Value = Counts.Find(Trigger);
	return Value ? *Value : 0;
}

void UHollowFeelSubsystem::ResetCounts()
{
	Counts.Reset();
	LastScale = 0.f;
}

FString UHollowFeelSubsystem::Describe() const
{
	TArray<FString> Felt;
	for (const TPair<FGameplayTag, int32>& Pair : Counts)
	{
		Felt.Add(FString::Printf(TEXT("%s x%d"), *Pair.Key.ToString(), Pair.Value));
	}
	Felt.Sort();
	return FString::Printf(TEXT("%s, comfort %.2f, felt: %s"), Config ? *Config->GetName() : TEXT("no config"), GetComfortScale(),
		Felt.IsEmpty() ? TEXT("nothing") : *FString::Join(Felt, TEXT(", ")));
}

namespace HollowFeelSubsystemPrivate
{
	void Dump(UWorld* World)
	{
		const UHollowFeelSubsystem* Feel = UHollowFeelSubsystem::Get(World);
		UE_LOG(LogHollowCore, Display, TEXT("Hollow.Feel.Dump: %s"), Feel ? *Feel->Describe() : TEXT("no feel subsystem in this world"));
	}

	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Hollow.Feel.Dump"),
		TEXT("Logs the game feel this machine's players felt so far (GP-01c)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Dump));
}
