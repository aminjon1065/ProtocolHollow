#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"

/**
 * Integer-only PRNG of the sector generator: PCG32 (O'Neill 2014, pcg-c-basic). Bit-identical on every platform and
 * compiler, independent of engine changes (no float, no FMath::Rand, no FRandomStream; CLAUDE.md sec. 5).
 */
class FHollowRandom
{
public:
	/** PCG32 default stream (pcg32_srandom_r with initseq 721347520444481703). */
	static constexpr uint64 DefaultStream = 721347520444481703ULL;

	explicit FHollowRandom(const uint64 Seed, const uint64 Stream = DefaultStream)
		: Increment((Stream << 1u) | 1u)
	{
		Next();
		State += Seed;
		Next();
	}

	uint32 Next()
	{
		const uint64 Old = State;
		State = Old * Multiplier + Increment;
		const uint32 XorShifted = static_cast<uint32>(((Old >> 18u) ^ Old) >> 27u);
		const uint32 Rotation = static_cast<uint32>(Old >> 59u);
		return (XorShifted >> Rotation) | (XorShifted << ((0u - Rotation) & 31u));
	}

	/** Uniform in [0, Bound) without modulo bias; Bound must be positive. */
	uint32 Below(const uint32 Bound)
	{
		check(Bound > 0u);
		const uint32 Threshold = (0u - Bound) % Bound;
		for (;;)
		{
			const uint32 Value = Next();
			if (Value >= Threshold)
			{
				return Value % Bound;
			}
		}
	}

	/** Uniform in [Min, Max]. */
	int32 RangeInclusive(const int32 Min, const int32 Max)
	{
		check(Min <= Max);
		return Min + static_cast<int32>(Below(static_cast<uint32>(Max - Min) + 1u));
	}

	/** Fisher-Yates shuffle. */
	template <typename T>
	void Shuffle(TArray<T>& Items)
	{
		for (int32 Index = Items.Num() - 1; Index > 0; --Index)
		{
			Items.Swap(Index, static_cast<int32>(Below(static_cast<uint32>(Index) + 1u)));
		}
	}

private:
	static constexpr uint64 Multiplier = 6364136223846793005ULL;   // PCG32 LCG multiplier

	uint64 State = 0;
	uint64 Increment = 0;
};

namespace HollowRandom
{
	/** SplitMix64 finalizer (Steele et al. 2014): a well-mixed 64-bit value. */
	constexpr uint64 Mix(uint64 Value)
	{
		Value += 0x9E3779B97F4A7C15ULL;
		Value = (Value ^ (Value >> 30u)) * 0xBF58476D1CE4E5B9ULL;
		Value = (Value ^ (Value >> 27u)) * 0x94D049BB133111EBULL;
		return Value ^ (Value >> 31u);
	}

	/** Seed of retry attempt N of a sector seed (ADR-0003: retries stay deterministic). */
	constexpr uint64 DeriveSeed(const int32 Seed, const int32 Attempt)
	{
		return Mix(Mix(static_cast<uint64>(static_cast<uint32>(Seed))) ^ static_cast<uint64>(static_cast<uint32>(Attempt)));
	}
}
