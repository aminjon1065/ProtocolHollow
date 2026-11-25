#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/HollowBiomeDefinition.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"

namespace HollowBiomeTests
{
	FHollowSocketActorRule Rule(const FGameplayTag& Type, const int32 Max = 0, const EHollowSocketPick Pick = EHollowSocketPick::ManifestOrder,
		const int32 MinRoomDistance = 0)
	{
		FHollowSocketActorRule Result;
		Result.SocketType = Type;
		Result.MaxPerSector = Max;
		Result.Pick = Pick;
		Result.MinRoomDistance = MinRoomDistance;
		return Result;
	}

	FHollowSocketSlot Slot(const FGameplayTag& Type, const int32 Distance)
	{
		FHollowSocketSlot Result;
		Result.SocketType = Type;
		Result.RoomDistance = Distance;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBiomeSocketRulesTest, "Hollow.PCG.Biome.SocketRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBiomeSocketRulesTest::RunTest(const FString& Parameters)
{
	// S3-04: one Iron Pilgrim per sector, at the EnemySpawn socket farthest from Spawn; the other sockets keep the
	// next rule of their type (dev markers); ties go by manifest order, so the same seed spawns at the same socket.
	using namespace HollowBiomeTests;
	UHollowBiomeDefinition* Biome = NewObject<UHollowBiomeDefinition>(GetTransientPackage());
	const FGameplayTag Enemy = HollowTags::Socket_EnemySpawn;
	const FGameplayTag Loot = HollowTags::Socket_Loot;
	Biome->SocketActors = {Rule(Enemy, 1, EHollowSocketPick::FarthestFromSpawn), Rule(Enemy), Rule(Loot)};
	constexpr int32 Pilgrim = 0, Marker = 1, Crate = 2;

	const TArray<FHollowSocketSlot> Slots = {Slot(Enemy, 2), Slot(Loot, 1), Slot(Enemy, 5), Slot(Enemy, 5), Slot(Enemy, INDEX_NONE)};
	const TArray<int32> Assigned = Biome->AssignSocketRules(Slots);
	TestEqual(TEXT("One rule per slot"), Assigned.Num(), Slots.Num());
	TestEqual(TEXT("The farthest socket gets the Pilgrim"), Assigned[2], Pilgrim);
	TestEqual(TEXT("A tie goes to the first in manifest order"), Assigned[3], Marker);
	TestTrue(TEXT("The rest of the type gets the next rule"), Assigned[0] == Marker && Assigned[4] == Marker);
	TestEqual(TEXT("Other types are untouched"), Assigned[1], Crate);
	TestEqual(TEXT("Exactly one Pilgrim"), Assigned.FilterByPredicate([](const int32 R) { return R == Pilgrim; }).Num(), 1);
	TestTrue(TEXT("Deterministic"), Biome->AssignSocketRules(Slots) == Assigned);

	Biome->SocketActors = {Rule(Enemy, 2)};
	const TArray<int32> FirstTwo = Biome->AssignSocketRules(Slots);
	TestTrue(TEXT("Manifest order takes the first ones"), FirstTwo[0] == 0 && FirstTwo[2] == 0 && FirstTwo[3] == INDEX_NONE && FirstTwo[4] == INDEX_NONE);
	TestEqual(TEXT("No rule, no actor"), FirstTwo[1], INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBiomeMinRoomDistanceTest, "Hollow.PCG.Biome.MinRoomDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBiomeMinRoomDistanceTest::RunTest(const FString& Parameters)
{
	// GP-02: the Pilgrim takes the farthest EnemySpawn socket, then up to two Whisperer nests the first sockets at
	// least two rooms from Spawn; what is nearer, or left over, goes to the markers.
	using namespace HollowBiomeTests;
	UHollowBiomeDefinition* Biome = NewObject<UHollowBiomeDefinition>(GetTransientPackage());
	const FGameplayTag Enemy = HollowTags::Socket_EnemySpawn;
	Biome->SocketActors = {Rule(Enemy, 1, EHollowSocketPick::FarthestFromSpawn), Rule(Enemy, 2, EHollowSocketPick::ManifestOrder, 2), Rule(Enemy)};
	constexpr int32 Pilgrim = 0, Nest = 1, Marker = 2;

	const TArray<FHollowSocketSlot> Slots = {Slot(Enemy, 1), Slot(Enemy, 0), Slot(Enemy, 2), Slot(Enemy, 6), Slot(Enemy, INDEX_NONE), Slot(Enemy, 3)};
	const TArray<int32> Assigned = Biome->AssignSocketRules(Slots);
	TestEqual(TEXT("The farthest socket gets the Pilgrim"), Assigned[3], Pilgrim);
	TestTrue(TEXT("Next to Spawn: no nest"), Assigned[0] == Marker && Assigned[1] == Marker);
	TestEqual(TEXT("Two links away: a nest"), Assigned[2], Nest);
	TestEqual(TEXT("No route from Spawn counts as far: a nest"), Assigned[4], Nest);
	TestEqual(TEXT("Past MaxPerSector: the marker"), Assigned[5], Marker);
	TestTrue(TEXT("Deterministic"), Biome->AssignSocketRules(Slots) == Assigned);

	Biome->SocketActors = {Rule(Enemy, 0, EHollowSocketPick::ManifestOrder, 3)};
	const TArray<int32> Every = Biome->AssignSocketRules(Slots);
	TestTrue(TEXT("Without a maximum: every far enough socket"), Every[3] == 0 && Every[4] == 0 && Every[5] == 0);
	TestTrue(TEXT("...and none nearer"), Every[0] == INDEX_NONE && Every[1] == INDEX_NONE && Every[2] == INDEX_NONE);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
