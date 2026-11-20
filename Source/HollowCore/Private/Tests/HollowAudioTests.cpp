#include "Audio/HollowSoundBank.h"
#include "Misc/AutomationTest.h"
#include "Noise/HollowNoiseConfig.h"
#include "Sector/HollowSectorTopology.h"
#include "Sound/SoundBase.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HollowAudioTests
{
	// Test data: a listener hearing 30 m; welding (loudness 1, capped at 15 m), footsteps (0.25, no cap); a players-only
	// door servo heard 15 m; a blast linked to the welding rule; walls letting 60 % of a noise's range through.
	constexpr float Reference = 3000.f;
	constexpr float WelderLoudness = 1.f;
	constexpr float WelderCap = 1500.f;
	constexpr float FootstepLoudness = 0.25f;
	constexpr float DoorRange = 1500.f;
	constexpr float InterRoom = 0.6f;
	constexpr float SprintScale = 1.5f;
	/** A replicated moment counts as fresh this long; test times (s). */
	constexpr float FreshSeconds = 1.f;
	constexpr double EventTime = 100.0;
	constexpr float Tolerance = 0.01f;
	// Two rooms of 1000 x 1000 side by side along X.
	constexpr double RoomSize = 1000.0;
	constexpr double RoomHeight = 400.0;
	const TCHAR* const SomeSound = TEXT("/Game/Hollow/Tests/MS_Test.MS_Test");

	UHollowNoiseConfig* MakeNoise()
	{
		UHollowNoiseConfig* Noise = NewObject<UHollowNoiseConfig>(GetTransientPackage());
		FHollowNoiseRule& Welder = Noise->Rules.AddDefaulted_GetRef();
		Welder.NoiseTag = HollowTags::Noise_Tool_Welder;
		Welder.Loudness = WelderLoudness;
		Welder.MaxRange = WelderCap;
		FHollowNoiseRule& Footstep = Noise->Rules.AddDefaulted_GetRef();
		Footstep.NoiseTag = HollowTags::Noise_Footstep;
		Footstep.Loudness = FootstepLoudness;
		Noise->InterRoomRange = InterRoom;
		return Noise;
	}

	FHollowSoundEntry Entry(const FGameplayTag& Tag, const float AudibleRange = 0.f, const FGameplayTag& Noise = FGameplayTag())
	{
		FHollowSoundEntry Out;
		Out.SoundTag = Tag;
		Out.Sound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(SomeSound));
		Out.AudibleRange = AudibleRange;
		Out.Noise = Noise;
		return Out;
	}

	TArray<FHollowSoundEntry> GoodBank()
	{
		return {Entry(HollowTags::Noise_Tool_Welder), Entry(HollowTags::Noise_Footstep), Entry(HollowTags::Sound_Door_Servo, DoorRange),
			Entry(HollowTags::Sound_Mine_Blast, 0.f, HollowTags::Noise_Tool_Welder)};
	}

	FHollowSectorTopology TwoRooms()
	{
		FHollowSectorTopology Topology;
		Topology.Rooms.Add({FBox(FVector::ZeroVector, FVector(RoomSize, RoomSize, RoomHeight))});
		Topology.Rooms.Add({FBox(FVector(RoomSize, 0.0, 0.0), FVector(2.0 * RoomSize, RoomSize, RoomHeight))});
		return Topology;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowAudioBankTest, "Hollow.Core.Audio.Bank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowAudioBankTest::RunTest(const FString& Parameters)
{
	// The bank's rules (ADR-0022): every noise the AI hears has a sound, and carries for players exactly as far as for
	// the reference enemy; players-only sounds carry their own range; a linked sound shares its noise's. Each broken
	// rule is reported once by tag.
	using namespace HollowAudioTests;
	const UHollowNoiseConfig* Noise = MakeNoise();
	const TArray<FHollowSoundEntry> Bank = GoodBank();
	TestTrue(FString::Printf(TEXT("A consistent bank passes: %s"), *FString::Join(HollowSoundRules::Check(Bank, *Noise), TEXT("; "))),
		HollowSoundRules::Check(Bank, *Noise).IsEmpty());

	TestEqual(TEXT("Welding: loudness x reference, capped by its MaxRange"), HollowSoundRules::AudibleRange(Bank[0], *Noise, Reference), WelderCap, Tolerance);
	TestEqual(TEXT("Footsteps: loudness x reference, no cap"), HollowSoundRules::AudibleRange(Bank[1], *Noise, Reference), FootstepLoudness * Reference, Tolerance);
	TestEqual(TEXT("A players-only sound: its own range"), HollowSoundRules::AudibleRange(Bank[2], *Noise, Reference), DoorRange, Tolerance);
	TestEqual(TEXT("A linked sound: its noise's range"), HollowSoundRules::AudibleRange(Bank[3], *Noise, Reference), WelderCap, Tolerance);
	TestTrue(TEXT("A noise entry is its own noise"), Bank[0].GetNoiseTag() == HollowTags::Noise_Tool_Welder);
	TestFalse(TEXT("A players-only sound has none"), Bank[2].GetNoiseTag().IsValid());
	TestTrue(TEXT("A linked sound has its link"), Bank[3].GetNoiseTag() == HollowTags::Noise_Tool_Welder);
	TestEqual(TEXT("A noise without a rule carries nowhere"), HollowSoundRules::AudibleRange(Entry(HollowTags::Noise_Core_Drop), *Noise, Reference), 0.f);

	UHollowAudioSettings* Settings = NewObject<UHollowAudioSettings>(GetTransientPackage());
	Settings->FreshEventSeconds = FreshSeconds;
	TestTrue(TEXT("Fresh: within the window"), Settings->IsFresh(EventTime, EventTime + FreshSeconds * 0.5));
	TestFalse(TEXT("Old news: past the window (a late arrival)"), Settings->IsFresh(EventTime, EventTime + FreshSeconds * 1.5));
	TestFalse(TEXT("Never happened (time 0)"), Settings->IsFresh(0.0, FreshSeconds * 0.5));

	struct FCase
	{
		const TCHAR* What;
		TArray<FHollowSoundEntry> Entries;
		FGameplayTag Named;
	};
	TArray<FCase> Cases;
	Cases.Add({TEXT("A noise rule without a sound"), {Bank[0], Bank[2]}, HollowTags::Noise_Footstep});
	TArray<FHollowSoundEntry> Silent = Bank;
	Silent[2].Sound.Reset();
	Cases.Add({TEXT("An entry without a sound"), Silent, HollowTags::Sound_Door_Servo});
	Cases.Add({TEXT("A tag outside Hollow.Noise / Hollow.Sound"), Bank, HollowTags::State_Dead});
	Cases.Last().Entries.Add(Entry(HollowTags::State_Dead, DoorRange));
	Cases.Add({TEXT("A tag listed twice"), Bank, HollowTags::Sound_Door_Servo});
	Cases.Last().Entries.Add(Bank[2]);
	Cases.Add({TEXT("A link to a noise without a rule"), Bank, HollowTags::Sound_Core_Hum});
	Cases.Last().Entries.Add(Entry(HollowTags::Sound_Core_Hum, 0.f, HollowTags::Noise_Core_Drop));
	Cases.Add({TEXT("A players-only sound without a range"), Bank, HollowTags::Sound_Mine_Arm});
	Cases.Last().Entries.Add(Entry(HollowTags::Sound_Mine_Arm));
	TArray<FHollowSoundEntry> TwoNumbers = Bank;
	TwoNumbers[0].AudibleRange = DoorRange;
	Cases.Add({TEXT("A noise with its own range as well"), TwoNumbers, HollowTags::Noise_Tool_Welder});
	for (const FCase& Case : Cases)
	{
		const TArray<FString> Problems = HollowSoundRules::Check(Case.Entries, *Noise);
		TestTrue(FString::Printf(TEXT("%s: one problem naming %s (%s)"), Case.What, *Case.Named.ToString(), *FString::Join(Problems, TEXT("; "))),
			Problems.Num() == 1 && Problems[0].Contains(Case.Named.ToString()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowNoiseThroughWallsTest, "Hollow.Core.Noise.ThroughWalls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowNoiseThroughWallsTest::RunTest(const FString& Parameters)
{
	// Noise and walls (ADR-0022): a noise's range for a listener (the hearing sense's formula, by loudness scale) and its
	// InterRoomRange share from another room of the topology; the same room or the open is left to the sense.
	using namespace HollowAudioTests;
	const UHollowNoiseConfig* Noise = MakeNoise();
	const FHollowNoiseRule& Welder = Noise->Rules[0];
	const FHollowNoiseRule& Footstep = Noise->Rules[1];
	TestEqual(TEXT("Range: capped"), Noise->RangeFor(Welder, Reference), WelderCap, Tolerance);
	TestEqual(TEXT("Range: by loudness scale (sprinting)"), Noise->RangeFor(Footstep, Reference, SprintScale), FootstepLoudness * SprintScale * Reference, Tolerance);

	const FHollowSectorTopology Topology = TwoRooms();
	const float Through = WelderCap * InterRoom;
	const FVector Source(RoomSize - 100.0, RoomSize * 0.5, RoomHeight * 0.5);
	auto Listener = [&Source](const double Distance) { return Source + FVector(Distance, 0.0, 0.0); };
	TestTrue(TEXT("Next room, within the walls' share"), Noise->ReachesThroughWalls(Topology, Welder, Reference, 1.f, Source, Listener(Through * 0.9)));
	TestFalse(TEXT("Next room, beyond it (though within the open range)"), Noise->ReachesThroughWalls(Topology, Welder, Reference, 1.f, Source, Listener(Through * 1.1)));
	TestTrue(TEXT("Same room: the sense decides"), Noise->ReachesThroughWalls(Topology, Welder, Reference, 1.f, Source, Source - FVector(WelderCap * 0.5, 0.0, 0.0)));
	TestTrue(TEXT("Outside every room: the sense decides"), Noise->ReachesThroughWalls(Topology, Welder, Reference, 1.f, Source, FVector(0.0, 0.0, RoomHeight * 3.0)));
	TestTrue(TEXT("No topology: the sense decides"), Noise->ReachesThroughWalls(FHollowSectorTopology(), Welder, Reference, 1.f, Source, Listener(Through * 1.1)));
	const float Steps = FootstepLoudness * Reference * InterRoom;
	TestTrue(TEXT("Walking through the wall, close"), Noise->ReachesThroughWalls(Topology, Footstep, Reference, 1.f, Source, Listener(Steps * 0.9)));
	TestFalse(TEXT("...a little further: muffled"), Noise->ReachesThroughWalls(Topology, Footstep, Reference, 1.f, Source, Listener(Steps * 1.1)));
	TestTrue(TEXT("...sprinting carries that far"), Noise->ReachesThroughWalls(Topology, Footstep, Reference, SprintScale, Source, Listener(Steps * 1.1)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
