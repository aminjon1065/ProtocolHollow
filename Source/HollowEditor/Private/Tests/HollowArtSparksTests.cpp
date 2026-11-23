#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "FX/HollowNiagaraAuthoring.h"
#include "Materials/Material.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"

namespace HollowArtSparksTests
{
	// Content under test (create_art_kit.py SPARKS, ADR-0043).
	const TCHAR* const SparksPath = TEXT("/Game/Hollow/FX/NS_Tool_BeamSparks.NS_Tool_BeamSparks");
	const TCHAR* const SparkMaterialPath = TEXT("/Game/Hollow/Art/Materials/M_Kit_Spark.M_Kit_Spark");

	/** Budget: sparks alive at once per running tool (spawn rate x longest life), so four tools stay a light CPU load. */
	constexpr double MaxSparksAlive = 300.0;
	/** Sparks bounce off the floor a few times and then rest; they never vanish on the first touch. */
	constexpr int32 MinBounces = 1;
	constexpr int32 MaxBounces = 4;

	/** The value of the first constant whose name ends with Suffix ("" if none). */
	FString Value(const TArray<FString>& Lines, const FString& Suffix)
	{
		const FString Key = Suffix + TEXT(" = ");
		for (const FString& Line : Lines)
		{
			const int32 At = Line.Find(Key, ESearchCase::CaseSensitive);
			if (At != INDEX_NONE && (At == 0 || Line[At - 1] == TEXT('.')))
			{
				return Line.Mid(At + Key.Len());
			}
		}
		return FString();
	}

	double Number(const TArray<FString>& Lines, const FString& Suffix)
	{
		return FCString::Atod(*Value(Lines, Suffix));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtSparksTest, "Hollow.Editor.ArtSparks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtSparksTest::RunTest(const FString& Parameters)
{
	// AK-8: the beam's sparks are one CPU emitter that falls under gravity and bounces off what it hits (ray-traced
	// collision that keeps the particle) within the budget; orange-white streaks along their flight (an additive, unlit
	// material on a velocity-aligned sprite).
	using namespace HollowArtSparksTests;
	UNiagaraSystem* Sparks = LoadObject<UNiagaraSystem>(nullptr, SparksPath);
	const UMaterial* SparkMaterial = LoadObject<UMaterial>(nullptr, SparkMaterialPath);
	if (!TestNotNull(TEXT("NS_Tool_BeamSparks"), Sparks) || !TestNotNull(TEXT("M_Kit_Spark"), SparkMaterial))
	{
		return false;
	}
	const TArray<FString> Lines = UHollowNiagaraAuthoring::Describe(Sparks);
	const int32 Emitters = Lines.FilterByPredicate([](const FString& Line) { return Line.StartsWith(TEXT("emitter ")); }).Num();
	TestTrue(TEXT("One emitter, simulated on the CPU (collision traces)"), Emitters == 1 && Lines.Contains(TEXT("emitter Sparks (CPU)")));
	TestTrue(TEXT("...with gravity and the collision module"), Lines.Contains(TEXT("module GravityForce")) && Lines.Contains(TEXT("module Collision")));
	FVector3f Gravity = FVector3f::ZeroVector;
	TestTrue(FString::Printf(TEXT("They fall (%s)"), *Value(Lines, TEXT("GravityForce.Gravity"))), Gravity.InitFromString(Value(Lines, TEXT("GravityForce.Gravity")))
		&& Gravity.Z < 0.f);
	const int32 Bounces = FCString::Atoi(*Value(Lines, TEXT("Collision.Max Number Of Collisions")));
	TestTrue(TEXT("They bounce: a hit keeps them"), Value(Lines, TEXT("Collision.Kill On Collision")) == TEXT("0"));
	TestTrue(FString::Printf(TEXT("...%d times, with some spring (restitution %.2f)"), Bounces, Number(Lines, TEXT("Collision.Restitution"))),
		Bounces >= MinBounces && Bounces <= MaxBounces && Number(Lines, TEXT("Collision.Restitution")) > 0.0);
	const double Alive = Number(Lines, TEXT("SpawnRate.SpawnRate")) * Number(Lines, TEXT("InitializeParticle.Lifetime Max"));
	TestTrue(FString::Printf(TEXT("Within the budget: %.0f alive at most (budget %.0f)"), Alive, MaxSparksAlive), Alive > 0.0 && Alive <= MaxSparksAlive);
	TestTrue(TEXT("...launched slower than the solver's speed limit"), Number(Lines, TEXT("RandomRangeFloat.Maximum")) <= Number(Lines, TEXT("SolveForcesAndVelocity.Speed Limit")));

	int32 Sprites = 0;
	for (const FNiagaraEmitterHandle& Handle : Sparks->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		for (const UNiagaraRendererProperties* Renderer : Data ? Data->GetRenderers() : TArray<UNiagaraRendererProperties*>())
		{
			if (const UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				++Sprites;
				TestTrue(TEXT("Streaks along their flight (M_Kit_Spark, velocity aligned)"), Sprite->Material == SparkMaterial
					&& Sprite->Alignment == ENiagaraSpriteAlignment::VelocityAligned);
			}
		}
	}
	TestEqual(TEXT("One sprite renderer"), Sprites, 1);
	TestTrue(TEXT("M_Kit_Spark: additive, unlit, for Niagara sprites"), SparkMaterial->BlendMode == BLEND_Additive
		&& SparkMaterial->GetShadingModels().HasOnlyShadingModel(MSM_Unlit) && SparkMaterial->GetUsageByFlag(MATUSAGE_NiagaraSprites));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
