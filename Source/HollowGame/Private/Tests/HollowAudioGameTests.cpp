#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Audio/HollowAudioSubsystem.h"
#include "Audio/HollowFootstepAudioComponent.h"
#include "Audio/HollowSoundBank.h"
#include "AudioGameplayVolume.h"
#include "Breach/HollowBreachableBulkhead.h"
#include "Breach/HollowBreachableDefinition.h"
#include "Breach/HollowStructuralIntegrityComponent.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarryableComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Components/AudioComponent.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowToolDefinition.h"
#include "Doors/HollowDoorRules.h"
#include "Engine/Brush.h"
#include "Engine/Level.h"
#include "FilterVolumeComponent.h"
#include "Gadgets/HollowPulseMine.h"
#include "Gadgets/HollowPulseMineDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayCueNotify_Actor.h"
#include "Model.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "ReverbVolumeComponent.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Sound/AmbientSound.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundEffectSource.h"
#include "Tests/HollowDocksTestRooms.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowAudioGameTests
{
	using namespace HollowEnemyTests;

	// Content under test (create_audio_assets.py, create_noise_assets.py, create_resonance_assets.py, create_pilgrim_assets.py).
	const TCHAR* const NoiseConfigPath = TEXT("/Game/Hollow/Core/DA_NoiseConfig.DA_NoiseConfig");
	const TCHAR* const WorldSoundClass = TEXT("/Game/Hollow/Audio/SC_Hollow_World.SC_Hollow_World");
	/** NAR-01a: the radio's clicks take the comms bus (create_voice_assets.py). */
	const TCHAR* const VoiceSoundClass = TEXT("/Game/Hollow/Audio/Voice/SC_Hollow_Voice.SC_Hollow_Voice");
	const TCHAR* const RadioChain = TEXT("/Game/Hollow/Audio/Voice/SEC_Hollow_Radio.SEC_Hollow_Radio");
	const TCHAR* const PilgrimDefinition = TEXT("/Game/Hollow/Characters/Enemies/IronPilgrim/DA_Enemy_IronPilgrim.DA_Enemy_IronPilgrim");
	const TCHAR* const WhispererDefinition = TEXT("/Game/Hollow/Characters/Enemies/Whisperer/DA_Enemy_Whisperer.DA_Enemy_Whisperer");
	const TCHAR* const StalkerDefinition = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/DA_Enemy_EchoStalker.DA_Enemy_EchoStalker");
	const TCHAR* const StalkerClass = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/BP_EchoStalker.BP_EchoStalker_C");
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const MineClass = TEXT("/Game/Hollow/Gadgets/PulseMine/BP_PulseMine.BP_PulseMine_C");
	const TCHAR* const BulkheadClass = TEXT("/Game/Hollow/World/Kit/Bulkheads/BP_Bulkhead_Twin.BP_Bulkhead_Twin_C");
	const TCHAR* const VaultReverb = TEXT("/Game/Hollow/Audio/Reverb/RE_Vault.RE_Vault");
	const TCHAR* const CorridorReverb = TEXT("/Game/Hollow/Audio/Reverb/RE_Corridor.RE_Corridor");
	// The Reactor Cathedral (S7-03, create_audio_assets.py, create_cathedral_look.py).
	const TCHAR* const CathedralReverb = TEXT("/Game/Hollow/Audio/Reverb/RE_Cathedral.RE_Cathedral");
	const TCHAR* const CryptReverb = TEXT("/Game/Hollow/Audio/Reverb/RE_Crypt.RE_Crypt");
	const TCHAR* const CloisterReverb = TEXT("/Game/Hollow/Audio/Reverb/RE_Cloister.RE_Cloister");
	const TCHAR* const ReactorHum = TEXT("/Game/Hollow/Audio/MetaSounds/MS_Reactor_Hum.MS_Reactor_Hum");
	constexpr float CathedralDecaySeconds = 4.5f;   // the task's number
	constexpr float DecayTolerance = 0.05f;
	const TCHAR* const MetaSoundSourceClass = TEXT("/Script/MetasoundEngine.MetaSoundSource");
	const TCHAR* const ToolDefinitions[] = {
		TEXT("/Game/Hollow/Tools/Welder/DA_Tool_Welder.DA_Tool_Welder"),
		TEXT("/Game/Hollow/Tools/PlasmaCutter/DA_Tool_PlasmaCutter.DA_Tool_PlasmaCutter"),
		TEXT("/Game/Hollow/Tools/AetherLantern/DA_Tool_AetherLantern.DA_Tool_AetherLantern"),   // S6-02: its hum
	};
	constexpr float RangeTolerance = 1.f;
	/** A spawned volume's default cube brush is 200 uu across. */
	constexpr double BrushHalfSize = 100.0;
	constexpr double BoundsTolerance = 1.0;
	/** Test data: where the bulkhead stands (clear of the scene's door and rooms), a light blow (share of a zone's health). */
	const FVector BulkheadAt(FloorMaxX - 200.0, FloorMinY + 200.0, 0.0);
	constexpr float LightBlow = 0.5f;
	/** Test data: a walking speed, and how far beyond / within the walls' share of the welder's range the Pilgrim stands. */
	constexpr float WalkSpeed = 400.f;
	constexpr double MuffledFactor = 1.15;
	constexpr double ThroughFactor = 0.75;

	/** Every Hollow.Sound.* the code plays. */
	TArray<FGameplayTag> NativeSounds()
	{
		return {HollowTags::Sound_Door_Servo, HollowTags::Sound_Footstep_Heavy, HollowTags::Sound_Footstep_Stalker, HollowTags::Sound_Enemy_PilgrimWindup,
			HollowTags::Sound_Enemy_StalkerShriek, HollowTags::Sound_Stalker_Vent, HollowTags::Sound_Mine_Arm, HollowTags::Sound_Mine_Blast, HollowTags::Sound_Core_Hum,
			HollowTags::Sound_Ward_Hum, HollowTags::Sound_Arc_Telegraph, HollowTags::Sound_Arc_Discharge, HollowTags::Sound_Altar_Purified,
			HollowTags::Sound_Radio_Open, HollowTags::Sound_Radio_Close};
	}

	/** Sounds that run until stopped (tools while used, the core's hum); every other one ends by itself. */
	TArray<FGameplayTag> Loops()
	{
		return {HollowTags::Noise_Tool_Welder, HollowTags::Noise_Tool_Cutter, HollowTags::Noise_Tool_Lantern, HollowTags::Sound_Core_Hum, HollowTags::Sound_Ward_Hum,
			HollowTags::Sound_Whisperer_Whisper};
	}

	/** Plays of Tag since the call before (counts per tag, UHollowAudioSubsystem). */
	struct FPlays
	{
		const UHollowAudioSubsystem* Audio = nullptr;
		TMap<FGameplayTag, int32> Seen;

		int32 New(const FGameplayTag& Tag)
		{
			const int32 Now = Audio->GetPlayCount(Tag);
			int32& Before = Seen.FindOrAdd(Tag);
			const int32 Delta = Now - Before;
			Before = Now;
			return Delta;
		}
	};

	/**
	 * A room level's acoustics (ADR-0022): one Audio Gameplay Volume exactly over the room, with a reverb preset and walls
	 * that muffle what comes from outside, and a looping room tone filling it through SC_Hollow_World. Returns the
	 * volume's reverb preset and the tone's sound (null when they are missing).
	 */
	TPair<const UReverbEffect*, const USoundBase*> CheckRoomAcoustics(FAutomationTestBase& Test, const UHollowRoomDefinition& Room, const USoundClass* SoundClass,
		const UHollowLevelMetrics* Metrics)
	{
		const FString Name = Room.GetName();
		const UWorld* Level = Room.RoomLevel.LoadSynchronous();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s level"), *Name), Level) || !Level->PersistentLevel)
		{
			return {};
		}
		TArray<const AAudioGameplayVolume*> Volumes;
		TArray<const AAmbientSound*> Tones;
		for (const AActor* Actor : Level->PersistentLevel->Actors)
		{
			if (const AAudioGameplayVolume* Volume = Cast<AAudioGameplayVolume>(Actor))
			{
				Volumes.Add(Volume);
			}
			else if (const AAmbientSound* Tone = Cast<AAmbientSound>(Actor))
			{
				Tones.Add(Tone);
			}
		}
		if (!Test.TestEqual(*FString::Printf(TEXT("%s: one acoustics volume"), *Name), Volumes.Num(), 1) || !Test.TestEqual(*FString::Printf(TEXT("%s: one room tone"), *Name), Tones.Num(), 1))
		{
			return {};
		}
		// The level is loaded, not playing: its transforms are the root components' relative ones (room-local).
		const AAudioGameplayVolume* Volume = Volumes[0];
		const FVector Size(Room.Footprint.X * Metrics->CellSize, Room.Footprint.Y * Metrics->CellSize, Room.Footprint.Z * Metrics->CellHeight);
		const USceneComponent* Root = Volume->GetRootComponent();
		const FBox Brush = Volume->Brush ? Volume->Brush->Bounds.GetBox() : FBox(ForceInit);
		const FBox Covered(Root->GetRelativeLocation() + Brush.Min * Root->GetRelativeScale3D(), Root->GetRelativeLocation() + Brush.Max * Root->GetRelativeScale3D());
		Test.TestTrue(FString::Printf(TEXT("%s: the volume covers the room exactly (%s)"), *Name, *Covered.ToString()),
			Covered.Min.Equals(FVector::ZeroVector, BoundsTolerance) && Covered.Max.Equals(Size, BoundsTolerance)
			&& Brush.Max.Equals(FVector(BrushHalfSize), BoundsTolerance));
		const UReverbVolumeComponent* Reverb = Volume->FindComponentByClass<UReverbVolumeComponent>();
		const UFilterVolumeComponent* Walls = Volume->FindComponentByClass<UFilterVolumeComponent>();
		Test.TestTrue(FString::Printf(TEXT("%s: a reverb preset"), *Name), Reverb && Reverb->GetReverbSettings().bApplyReverb && Reverb->GetReverbSettings().ReverbEffect);
		Test.TestTrue(FString::Printf(TEXT("%s: what comes from outside is muffled"), *Name), Walls && Walls->GetExteriorLPF() < MAX_FILTER_FREQUENCY);
		const UAudioComponent* Tone = Tones[0]->GetAudioComponent();
		const USoundBase* Sound = Tone ? Tone->Sound.Get() : nullptr;
		Test.TestTrue(FString::Printf(TEXT("%s: a looping room tone through SC_Hollow_World"), *Name), Sound && !Sound->IsOneShot() && Sound->SoundClassObject == SoundClass);
		Test.TestTrue(FString::Printf(TEXT("%s: ...filling the room"), *Name), Tone && Tone->bOverrideAttenuation
			&& Tone->AttenuationOverrides.AttenuationShape == EAttenuationShape::Box && Tone->AttenuationOverrides.AttenuationShapeExtents.Equals(Size * 0.5, BoundsTolerance));
		return TPair<const UReverbEffect*, const USoundBase*>(Reverb ? Reverb->GetReverbSettings().ReverbEffect.Get() : nullptr, Sound);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAudioConsistencyTest, "Hollow.Game.Audio.Consistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAudioConsistencyTest::RunTest(const FString& Parameters)
{
	// The shipped sound base (ADR-0022): DA_SoundBank agrees with DA_NoiseConfig, so what players hear of a noise
	// carries exactly as far as the Iron Pilgrim hears it; every sound the code plays is there, each a procedural
	// MetaSound through SC_Hollow_World (the Resonance low-pass and the room volumes apply), looping or ending as used;
	// only the handler's radio clicks take the comms bus (NAR-01a: the helmet's radio, no room muffles it).
	using namespace HollowAudioGameTests;
	const UHollowSoundBank* Bank = UHollowSoundBank::Get();
	const UHollowNoiseConfig* Noise = LoadObject<UHollowNoiseConfig>(nullptr, NoiseConfigPath);
	const USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, WorldSoundClass);
	const USoundClass* VoiceClass = LoadObject<USoundClass>(nullptr, VoiceSoundClass);
	const USoundEffectSourcePresetChain* Radio = LoadObject<USoundEffectSourcePresetChain>(nullptr, RadioChain);
	const UHollowEnemyDefinition* Pilgrim = LoadObject<UHollowEnemyDefinition>(nullptr, PilgrimDefinition);
	const UHollowEnemyDefinition* Stalker = LoadObject<UHollowEnemyDefinition>(nullptr, StalkerDefinition);
	if (!TestNotNull(TEXT("DA_SoundBank (UHollowAudioSettings)"), Bank) || !TestNotNull(TEXT("DA_NoiseConfig"), Noise)
		|| !TestNotNull(TEXT("SC_Hollow_World"), SoundClass) || !TestNotNull(TEXT("SC_Hollow_Voice"), VoiceClass) || !TestNotNull(TEXT("SEC_Hollow_Radio"), Radio)
		|| !TestNotNull(TEXT("DA_Enemy_IronPilgrim"), Pilgrim) || !TestNotNull(TEXT("DA_Enemy_EchoStalker"), Stalker))
	{
		return false;
	}
	const TArray<FString> Problems = HollowSoundRules::Check(Bank->Entries, *Noise);
	TestTrue(FString::Printf(TEXT("The bank matches the noise rules%s%s"), Problems.IsEmpty() ? TEXT("") : TEXT(": "), *FString::Join(Problems, TEXT("; "))), Problems.IsEmpty());

	const float Reference = GetDefault<UHollowAudioSettings>()->ReferenceHearingRange;
	TestEqual(TEXT("Players hear a noise as far as the Iron Pilgrim does (the reference)"), Reference, Pilgrim->HearingRange);
	TestTrue(TEXT("Late arrivals are told by a freshness window"), GetDefault<UHollowAudioSettings>()->FreshEventSeconds > 0.f);
	for (const FHollowNoiseRule& Rule : Noise->Rules)
	{
		if (Rule.NoiseTag.MatchesTag(HollowTags::Noise_Voice))
		{
			TestNull(*FString::Printf(TEXT("%s: heard through VOIP, not the bank (S4-07)"), *Rule.NoiseTag.ToString()), Bank->Find(Rule.NoiseTag));
			continue;
		}
		const FHollowSoundEntry* Entry = Bank->Find(Rule.NoiseTag);
		TestTrue(FString::Printf(TEXT("%s: a sound carrying as far as the Pilgrim hears it"), *Rule.NoiseTag.ToString()),
			Entry && FMath::IsNearlyEqual(HollowSoundRules::AudibleRange(*Entry, *Noise, Reference), Noise->RangeFor(Rule, Pilgrim->HearingRange), RangeTolerance));
	}
	for (const FGameplayTag& Tag : NativeSounds())
	{
		TestNotNull(*FString::Printf(TEXT("%s is in the bank"), *Tag.ToString()), Bank->Find(Tag));
	}
	TestTrue(TEXT("Walls let part of a noise through (0 < InterRoomRange < 1)"), Noise->InterRoomRange > 0.f && Noise->InterRoomRange < 1.f);
	TestTrue(TEXT("The room volumes apply to SC_Hollow_World"), SoundClass->Properties.bApplyAmbientVolumes);
	for (const FHollowSoundEntry& Entry : Bank->Entries)
	{
		const FString Name = Entry.SoundTag.ToString();
		const USoundBase* Sound = Entry.Sound.LoadSynchronous();
		if (!TestNotNull(*FString::Printf(TEXT("%s: its sound loads"), *Name), Sound))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s: a procedural MetaSound (no wave bank)"), *Name), Sound->GetClass()->GetPathName() == MetaSoundSourceClass);
		if (Entry.SoundTag == HollowTags::Sound_Radio_Open || Entry.SoundTag == HollowTags::Sound_Radio_Close)
		{
			TestTrue(FString::Printf(TEXT("%s: through the comms bus (SC_Hollow_Voice, SEC_Hollow_Radio)"), *Name),
				Sound->SoundClassObject == VoiceClass && Sound->SourceEffectChain == Radio);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("%s: through SC_Hollow_World"), *Name), Sound->SoundClassObject == SoundClass);
		}
		const bool bLoop = Loops().Contains(Entry.SoundTag);
		TestEqual(FString::Printf(TEXT("%s: %s"), *Name, bLoop ? TEXT("loops until stopped") : TEXT("ends by itself")), Sound->IsOneShot(), !bLoop);
		TestNotNull(*FString::Printf(TEXT("%s: an attenuation"), *Name), Entry.Attenuation.LoadSynchronous());
	}
	for (const TCHAR* Path : ToolDefinitions)
	{
		const UHollowToolDefinition* Tool = LoadObject<UHollowToolDefinition>(nullptr, Path);
		TestTrue(FString::Printf(TEXT("%s: its noise has a sound"), Path), Tool && Bank->Find(Tool->NoiseTag));
	}
	const UHollowEnemyDefinition* Whisperer = LoadObject<UHollowEnemyDefinition>(nullptr, WhispererDefinition);   // GP-02
	if (!TestNotNull(TEXT("DA_Enemy_Whisperer"), Whisperer))
	{
		return false;
	}
	for (const UHollowEnemyDefinition* Enemy : {Pilgrim, Stalker, Whisperer})
	{
		TestTrue(FString::Printf(TEXT("%s: wind-up and footstep sounds in the bank"), *Enemy->GetName()), Bank->Find(Enemy->WindupSound) && Bank->Find(Enemy->FootstepSound));
		TestTrue(FString::Printf(TEXT("%s: heard patrolling, loudest chasing"), *Enemy->GetName()), Enemy->FootstepCadence.VolumeAt(Enemy->PatrolSpeed) > 0.f
			&& Enemy->FootstepCadence.VolumeAt(Enemy->PatrolSpeed) < Enemy->FootstepCadence.VolumeAt(Enemy->ChaseSpeed));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAudioEventsTest, "Hollow.Game.Audio.Events",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAudioEventsTest::RunTest(const FString& Parameters)
{
	// Sound follows replicated state (ADR-0022), here on the host: a door's servo on each motion, a blow when it loses
	// structure, the crash of its breach; a bulkhead's blow and broken zone; the core's hum and its drop; the mine's beep when armed and its blast; the
	// stalker in a vent; an enemy's wind-up voice; footsteps by the noise's own rules (walking, not crouched or still).
	// The audio subsystem's ranges are the noise rules'.
	using namespace HollowAudioGameTests;
	FEnemyScene Scene;
	const TSubclassOf<AHollowAetherCore> Core = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
	const TSubclassOf<AHollowPulseMine> Mine = LoadClass<AHollowPulseMine>(nullptr, MineClass);
	const TSubclassOf<AGameplayCueNotify_Actor> Windup = LoadClass<AGameplayCueNotify_Actor>(nullptr, WindupCueClass);
	if (!TestNotNull(TEXT("BP_AetherCore"), Core.Get()) || !TestNotNull(TEXT("BP_PulseMine"), Mine.Get()) || !TestNotNull(TEXT("GCN_Enemy_Windup"), Windup.Get())
		|| !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	UHollowAudioSubsystem* Audio = UHollowAudioSubsystem::Get(Scene.World);
	const UHollowNoiseSubsystem* NoiseSubsystem = UHollowNoiseSubsystem::Get(Scene.World);
	const UHollowNoiseConfig* Noise = NoiseSubsystem ? NoiseSubsystem->GetConfig() : nullptr;
	if (!TestNotNull(TEXT("Audio subsystem in a game world"), Audio) || !TestNotNull(TEXT("Noise rules"), Noise))
	{
		return false;
	}
	const float Reference = GetDefault<UHollowAudioSettings>()->ReferenceHearingRange;
	for (const FHollowNoiseRule& Rule : Noise->Rules)
	{
		if (Rule.NoiseTag.MatchesTag(HollowTags::Noise_Voice))
		{
			continue;   // a voice is played by its VOIP talker (S4-07)
		}
		TestEqual(FString::Printf(TEXT("%s: heard as far as its rule carries"), *Rule.NoiseTag.ToString()), Audio->GetAudibleRange(Rule.NoiseTag),
			Noise->RangeFor(Rule, Reference), RangeTolerance);
	}
	TestEqual(TEXT("The blast carries as far as a heavy impact (its noise)"), Audio->GetAudibleRange(HollowTags::Sound_Mine_Blast),
		Audio->GetAudibleRange(HollowTags::Noise_Impact_Heavy), RangeTolerance);
	FPlays Plays{Audio};

	// The core: hums from the start; carried and put down, it thuds.
	AHollowExoFrameCharacter* Player = nullptr;
	if (!Scene.SpawnPlayer(*this, RoomA, Player))
	{
		return false;
	}
	AHollowAetherCore* Aether = Scene.World->SpawnActor<AHollowAetherCore>(Core, FTransform(RoomA + FVector(150.0, 0.0, -100.0)));
	TestEqual(TEXT("The core hums"), Plays.New(HollowTags::Sound_Core_Hum), 1);
	TestTrue(TEXT("Carried"), Aether && Aether->GetCarryable()->BeginSolo(Player));
	TestEqual(TEXT("...silent while lifted"), Plays.New(HollowTags::Noise_Core_Drop), 0);
	TestTrue(TEXT("Put down"), Aether && Aether->GetCarryable()->Release(FVector::ZeroVector, /*bForce=*/true));
	TestEqual(TEXT("...a thud"), Plays.New(HollowTags::Noise_Core_Drop), 1);

	// The door: a servo for each motion, a blow, the breach (and no blow for the breaking one).
	UHollowDoorStateComponent* Door = Scene.Door->FindComponentByClass<UHollowDoorStateComponent>();
	if (!TestNotNull(TEXT("Door state"), Door))
	{
		return false;
	}
	const float Transition = Door->GetDefinition()->OpenDuration;
	TestTrue(TEXT("Opens"), Door->Interact(/*bDoorwayClear=*/true) != EHollowDoorOutcome::Rejected);
	TestEqual(TEXT("...the servo"), Plays.New(HollowTags::Sound_Door_Servo), 1);
	Advance(Scene.TestWorld, Transition * 1.5f);
	TestEqual(TEXT("Open: no second servo on arrival"), Plays.New(HollowTags::Sound_Door_Servo), 0);
	TestTrue(TEXT("Closes"), Door->Interact(/*bDoorwayClear=*/true) != EHollowDoorOutcome::Rejected);
	TestEqual(TEXT("...the servo again"), Plays.New(HollowTags::Sound_Door_Servo), 1);
	Advance(Scene.TestWorld, Transition * 1.5f);
	Door->ApplyStructural(Door->GetDefinition()->StructureHitPoints * 0.25f, /*bBackwards=*/false);
	TestEqual(TEXT("A blow: a heavy impact"), Plays.New(HollowTags::Noise_Impact_Heavy), 1);
	Door->ApplyStructural(Door->GetDefinition()->StructureHitPoints * 2.f, /*bBackwards=*/false);
	TestEqual(TEXT("Breached: the crash"), Plays.New(HollowTags::Noise_Structural_Breach), 1);
	TestEqual(TEXT("...not a blow as well"), Plays.New(HollowTags::Noise_Impact_Heavy), 0);

	// The mine: a beep once armed, the blast.
	FActorSpawnParameters Params;
	Params.Instigator = Player;
	AHollowPulseMine* Pulse = Scene.World->SpawnActor<AHollowPulseMine>(Mine, FTransform(RoomA + FVector(0.0, 300.0, -100.0)), Params);
	if (!TestNotNull(TEXT("Mine"), Pulse))
	{
		return false;
	}
	TestEqual(TEXT("Laid: not armed, silent"), Plays.New(HollowTags::Sound_Mine_Arm), 0);
	Advance(Scene.TestWorld, Pulse->GetDefinition()->ArmSeconds * 1.5f);
	TestTrue(TEXT("Armed: one beep"), Pulse->IsArmed() && Plays.New(HollowTags::Sound_Mine_Arm) == 1);
	Pulse->Detonate();
	TestEqual(TEXT("The blast"), Plays.New(HollowTags::Sound_Mine_Blast), 1);

	// The bulkhead: a blow, then its zone breaks.
	const TSubclassOf<AHollowBreachableBulkhead> Bulkhead = LoadClass<AHollowBreachableBulkhead>(nullptr, BulkheadClass);
	AHollowBreachableBulkhead* Twin = Bulkhead ? Scene.World->SpawnActor<AHollowBreachableBulkhead>(Bulkhead, FTransform(BulkheadAt)) : nullptr;
	const UHollowBreachableDefinition* Breachable = Twin ? Twin->GetIntegrity()->GetDefinition() : nullptr;
	if (!TestNotNull(TEXT("BP_Bulkhead_Twin"), Breachable))
	{
		return false;
	}
	auto Blow = [Twin](const float Amount)
	{
		FHollowStructuralHit Hit;
		Hit.Amount = Amount;
		Hit.Location = Twin->GetZoneCenter(0);
		Hit.Direction = Twin->GetActorForwardVector();
		Hit.DamageType = HollowTags::Damage_Structural;
		return Twin->ApplyStructuralDamage(Hit);
	};
	TestTrue(TEXT("Bulkhead: a light blow"), Blow(Breachable->Zones[0].MaxHealth * LightBlow));
	TestEqual(TEXT("...a heavy impact"), Plays.New(HollowTags::Noise_Impact_Heavy), 1);
	TestTrue(TEXT("Its zone breaks"), Blow(Breachable->Zones[0].MaxHealth));
	TestEqual(TEXT("...the crash (fresh)"), Plays.New(HollowTags::Noise_Structural_Breach), 1);
	TestEqual(TEXT("...not a blow as well"), Plays.New(HollowTags::Noise_Impact_Heavy), 0);

	// Enemies: the vent, the wind-up voice, their own footsteps.
	AHollowEnemyCharacter* Stalker = Scene.SpawnEnemy(*this, StalkerClass, RoomB);
	AHollowEnemyCharacter* Pilgrim = Scene.SpawnPilgrim(*this, RoomB + FVector(0.0, -400.0, 0.0));
	if (!Stalker || !Pilgrim)
	{
		return false;
	}
	Stalker->EmergeFromVent();
	TestEqual(TEXT("Out of the vents (the director's spawn): the grille"), Plays.New(HollowTags::Sound_Stalker_Vent), 1);
	Stalker->SetInVent(true);
	TestEqual(TEXT("Into a vent: the grille"), Plays.New(HollowTags::Sound_Stalker_Vent), 1);
	Stalker->SetInVent(false);
	TestEqual(TEXT("...and out"), Plays.New(HollowTags::Sound_Stalker_Vent), 1);
	AGameplayCueNotify_Actor* Cue = Scene.World->SpawnActor<AGameplayCueNotify_Actor>(Windup, FTransform(Pilgrim->GetActorLocation()));
	if (TestNotNull(TEXT("The wind-up cue"), Cue))
	{
		Cue->OnActive(Pilgrim, FGameplayCueParameters());
	}
	TestEqual(TEXT("...the Pilgrim's voice (its definition)"), Plays.New(HollowTags::Sound_Enemy_PilgrimWindup), 1);
	UHollowFootstepAudioComponent* PilgrimSteps = Pilgrim->FindComponentByClass<UHollowFootstepAudioComponent>();
	const UHollowFootstepAudioComponent* StalkerSteps = Stalker->FindComponentByClass<UHollowFootstepAudioComponent>();
	TestTrue(TEXT("Enemies step with their own sounds"), PilgrimSteps && StalkerSteps && PilgrimSteps->SoundTag == HollowTags::Sound_Footstep_Heavy
		&& StalkerSteps->SoundTag == HollowTags::Sound_Footstep_Stalker);
	if (PilgrimSteps && Pilgrim->GetDefinition())
	{
		Pilgrim->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Pilgrim->GetCharacterMovement()->Velocity = FVector(Pilgrim->GetDefinition()->PatrolSpeed, 0.0, 0.0);
		TestTrue(TEXT("The Pilgrim's patrol is heard (its cadence, not a player's noise threshold)"), PilgrimSteps->Step());
		TestEqual(TEXT("...a heavy step"), Plays.New(HollowTags::Sound_Footstep_Heavy), 1);
	}
	Stalker->Destroy();
	Pilgrim->Destroy();

	// The player's footsteps: the noise's tag, by the noise's rules.
	UHollowFootstepAudioComponent* Steps = Player->FindComponentByClass<UHollowFootstepAudioComponent>();
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	if (!TestNotNull(TEXT("Footstep audio"), Steps))
	{
		return false;
	}
	TestTrue(TEXT("A player's steps are the noise the AI hears"), Steps->SoundTag == HollowTags::Noise_Footstep);
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = FVector::ZeroVector;
	TestFalse(TEXT("Standing still: no step"), Steps->Step());
	Movement->Velocity = FVector(Noise->FootstepMinSpeed * 0.5f, 0.0, 0.0);
	TestFalse(TEXT("Creeping under the minimum speed: no step"), Steps->Step());
	Movement->Velocity = FVector(WalkSpeed, 0.0, 0.0);
	Player->bIsCrouched = true;
	TestFalse(TEXT("Crouched: no step"), Steps->Step());
	Player->bIsCrouched = false;
	Plays.New(HollowTags::Noise_Footstep);
	TestTrue(TEXT("Walking: a step"), Steps->Step());
	TestEqual(TEXT("...one footstep sound"), Plays.New(HollowTags::Noise_Footstep), 1);
	Movement->SetMovementMode(MOVE_Falling);
	TestFalse(TEXT("In the air: no step"), Steps->Step());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAudioRoomAcousticsTest, "Hollow.Game.Audio.RoomAcoustics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAudioRoomAcousticsTest::RunTest(const FString& Parameters)
{
	// Every Docks room level (ADR-0022): one Audio Gameplay Volume exactly over the room, with a reverb preset and
	// walls that muffle what comes from outside, and a looping room tone filling it through SC_Hollow_World. The
	// hero vault rings longer than a corridor.
	using namespace HollowAudioGameTests;
	using namespace HollowDocksTestRooms;
	const USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, WorldSoundClass);
	const UReverbEffect* Vault = LoadObject<UReverbEffect>(nullptr, VaultReverb);
	const UReverbEffect* Corridor = LoadObject<UReverbEffect>(nullptr, CorridorReverb);
	const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
	const TArray<const UHollowRoomDefinition*> Rooms = DocksRoomDefinitions();
	if (!TestNotNull(TEXT("SC_Hollow_World"), SoundClass) || !TestNotNull(TEXT("RE_Vault"), Vault) || !TestNotNull(TEXT("RE_Corridor"), Corridor)
		|| !TestEqual(TEXT("Docks rooms"), Rooms.Num(), DocksRooms))
	{
		return false;
	}
	TestTrue(TEXT("The vault rings longer than a corridor"), Vault->DecayTime > Corridor->DecayTime);
	for (const UHollowRoomDefinition* Room : Rooms)
	{
		const TPair<const UReverbEffect*, const USoundBase*> Acoustics = CheckRoomAcoustics(*this, *Room, SoundClass, Metrics);
		if (Room->GetName() == HeroRoom)
		{
			TestTrue(TEXT("The hero vault rings as a vault"), Acoustics.Key == Vault);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAudioCathedralAcousticsTest, "Hollow.Game.Audio.CathedralAcoustics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAudioCathedralAcousticsTest::RunTest(const FString& Parameters)
{
	// S7-03 (ADR-0040): every Cathedral room is dressed like the Docks' (a volume over the room, two storeys high in the
	// halls, walls muffling the outside, a room tone filling it); the two-storey halls ring RE_Cathedral for 4.5 s, the
	// crypt RE_Crypt, the rest RE_Cloister; the room tone is the reactor's hum, loudest in its crypt.
	using namespace HollowAudioGameTests;
	using namespace HollowDocksTestRooms;
	const USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, WorldSoundClass);
	const UReverbEffect* Halls = LoadObject<UReverbEffect>(nullptr, CathedralReverb);
	const UReverbEffect* Crypt = LoadObject<UReverbEffect>(nullptr, CryptReverb);
	const UReverbEffect* Cloister = LoadObject<UReverbEffect>(nullptr, CloisterReverb);
	const USoundBase* Hum = LoadObject<USoundBase>(nullptr, ReactorHum);
	const TArray<const UHollowRoomDefinition*> Rooms = SectorRoomDefinitions(CathedralRoot);
	if (!TestNotNull(TEXT("SC_Hollow_World"), SoundClass) || !TestNotNull(TEXT("RE_Cathedral"), Halls) || !TestNotNull(TEXT("RE_Crypt"), Crypt)
		|| !TestNotNull(TEXT("RE_Cloister"), Cloister) || !TestNotNull(TEXT("MS_Reactor_Hum"), Hum) || !TestEqual(TEXT("Cathedral rooms"), Rooms.Num(), CathedralRooms))
	{
		return false;
	}
	TestNearlyEqual(TEXT("The halls ring 4.5 s"), Halls->DecayTime, CathedralDecaySeconds, DecayTolerance);
	TestTrue(TEXT("...longer than the crypt, the crypt longer than the cloisters"), Halls->DecayTime > Crypt->DecayTime && Crypt->DecayTime > Cloister->DecayTime);
	for (const UHollowRoomDefinition* Room : Rooms)
	{
		const TPair<const UReverbEffect*, const USoundBase*> Acoustics = CheckRoomAcoustics(*this, *Room, SoundClass, GetDefault<UHollowLevelMetrics>());
		const UReverbEffect* Expected = Room->Footprint.Z > 1 ? Halls : Room->RoomTags.HasTagExact(HollowTags::Room_Type_CoreVault) ? Crypt : Cloister;
		TestTrue(FString::Printf(TEXT("%s rings %s"), *Room->GetName(), *GetNameSafe(Expected)), Acoustics.Key == Expected);
		TestTrue(FString::Printf(TEXT("%s: the reactor's hum"), *Room->GetName()), Acoustics.Value == Hum);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAIInterRoomHearingTest, "Hollow.Game.AI.InterRoomHearing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAIInterRoomHearingTest::RunTest(const FString& Parameters)
{
	// Walls between rooms (ADR-0022): the S3-05 scene with a sector topology, room A and room B on either side of the
	// solid wall. Real welding in room A (the welder's sound plays with its beam); the Pilgrim in room B, further than
	// InterRoomRange of the welding range, does not hear it through the wall although it would in the open; nearer
	// than that, it does.
	using namespace HollowAudioGameTests;
	FEnemyScene Scene;
	const TSubclassOf<UGameplayAbility> WelderClass = LoadClass<UGameplayAbility>(nullptr, WelderAbilityClass);
	if (!TestNotNull(TEXT("GA_Tool_Welder"), WelderClass.Get()) || !TestNotNull(TEXT("GCN_Tool_Beam"), LoadClass<AActor>(nullptr, BeamCueClass))
		|| !Scene.Create(*this, EWall::Solid))
	{
		return false;
	}
	FHollowSectorTopology Topology;
	Topology.Rooms.Add({FBox(FVector(FloorMinX, FloorMinY, -SlabThickness), FVector(WallX, FloorMaxY, WallHeight))});
	Topology.Rooms.Add({FBox(FVector(WallX, FloorMinY, -SlabThickness), FVector(FloorMaxX, FloorMaxY, WallHeight))});
	UHollowSectorTopologySubsystem::Get(Scene.World)->SetTopology(Topology);

	AHollowExoFrameCharacter* Welder = nullptr;
	UHollowAbilitySystemComponent* WelderAbilities = Scene.SpawnPlayer(*this, WelderInRoomA, Welder, 0.f, /*bLocalPlayer=*/true);
	AHollowEnemyCharacter* Pilgrim = Scene.SpawnPilgrim(*this, RoomB);
	AHollowAIController* Brain = Pilgrim ? Cast<AHollowAIController>(Pilgrim->GetController()) : nullptr;
	const UHollowNoiseConfig* Noise = UHollowNoiseSubsystem::Get(Scene.World)->GetConfig();
	const FHollowNoiseRule* Rule = Noise ? Noise->FindRule(HollowTags::Noise_Tool_Welder) : nullptr;
	if (!WelderAbilities || !Brain || !TestNotNull(TEXT("Welding rule"), Rule))
	{
		return false;
	}
	const double Open = Noise->RangeFor(*Rule, Brain->GetHearingRange());
	const double Through = Open * Noise->InterRoomRange;
	const FVector Muffled = WelderInRoomA + FVector(Through * MuffledFactor, 0.0, 0.0);
	const FVector Heard = WelderInRoomA + FVector(Through * ThroughFactor, 0.0, 0.0);
	if (!TestTrue(TEXT("Test layout: both spots in room B, the muffled one within the open range"), Topology.RoomAt(Muffled) == 1 && Topology.RoomAt(Heard) == 1
		&& FVector::Dist(Muffled, WelderInRoomA) < Open))
	{
		return false;
	}
	Pilgrim->GetCharacterMovement()->DisableMovement();   // it listens where it is put (a patrol would walk it to the wall)
	Pilgrim->SetActorLocation(Muffled + FVector(0.0, 0.0, RoomB.Z - WelderInRoomA.Z));
	Welder->GetController()->SetControlRotation((FreeDoorInRoomA - WelderInRoomA).Rotation());   // facing the door it welds
	FGameplayAbilitySpec Spec(WelderClass, 1);
	Spec.GetDynamicSpecSourceTags().AddTag(HollowTags::Input_SecondaryUse);
	WelderAbilities->GiveAbility(Spec);
	FPlays Plays{UHollowAudioSubsystem::Get(Scene.World)};

	WelderAbilities->AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
	Advance(Scene.TestWorld, HearSeconds, [&] { return Brain->HasStimulus(); });
	TestEqual(TEXT("Welding: its sound plays (the beam's loop)"), Plays.New(HollowTags::Noise_Tool_Welder), 1);
	TestTrue(TEXT("...the Pilgrim still where it was put"), FVector::Dist2D(Pilgrim->GetActorLocation(), Muffled) < StimulusTolerance);
	TestFalse(TEXT("Beyond the walls' share: not heard through the wall"), Brain->HasStimulus() || Brain->GetActivity() == EHollowAIActivity::Investigate);

	Pilgrim->SetActorLocation(Heard + FVector(0.0, 0.0, RoomB.Z - WelderInRoomA.Z));
	Advance(Scene.TestWorld, HearSeconds, [&] { return Brain->HasStimulus(); });
	TestTrue(TEXT("Nearer: heard through the wall"), Brain->HasStimulus());
	TestTrue(TEXT("...where the welder is"), FVector::Dist2D(Brain->GetStimulusLocation(), Welder->GetActorLocation()) < StimulusTolerance);
	WelderAbilities->AbilityInputTagReleased(HollowTags::Input_SecondaryUse);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
