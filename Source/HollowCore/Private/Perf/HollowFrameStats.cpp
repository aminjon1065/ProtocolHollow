#include "Perf/HollowFrameStats.h"

#include "Algo/Accumulate.h"
#include "Algo/Count.h"
#include "Algo/MaxElement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowFrameStats)

namespace HollowFrameStatsPrivate
{
	/** The CSV profiler's last line holds the metadata; with this key the line before it is the full header. */
	const TCHAR* const HeaderAtEndKey = TEXT("[HasHeaderRowAtEnd]");
	constexpr float Median = 50.f;
	constexpr float Hundred = 100.f;

	const TArray<float>* Find(const TMap<FString, TArray<float>>& Columns, const TCHAR* Name)
	{
		const TArray<float>* Column = Columns.Find(Name);
		return Column && !Column->IsEmpty() ? Column : nullptr;
	}

	double Sum(const TArray<float>* Column)
	{
		return Column ? Algo::Accumulate(*Column, 0.0) : 0.0;
	}

	/** A timing stat of Category on any thread: "Category/Stat" (game thread) or "Category/<Thread>/Stat". */
	double SumOnAnyThread(const TMap<FString, TArray<float>>& Columns, const TCHAR* Category, const TCHAR* Stat)
	{
		const FString Prefix = FString(Category) + TEXT("/");
		const FString Suffix = FString(TEXT("/")) + Stat;
		double Total = 0.0;
		for (const TPair<FString, TArray<float>>& Column : Columns)
		{
			if (Column.Key.StartsWith(Prefix, ESearchCase::CaseSensitive) && Column.Key.EndsWith(Suffix, ESearchCase::CaseSensitive))
			{
				Total += Algo::Accumulate(Column.Value, 0.0);
			}
		}
		return Total;
	}
}

TMap<FString, TArray<float>> HollowFrameStats::ParseCsv(const FString& Text)
{
	using namespace HollowFrameStatsPrivate;
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, /*CullEmpty=*/true);
	TMap<FString, TArray<float>> Columns;
	if (Lines.Num() < 2)
	{
		return Columns;
	}
	int32 LastRow = Lines.Num() - 1;
	int32 HeaderLine = 0;
	if (Lines.Last().StartsWith(HeaderAtEndKey))
	{
		HeaderLine = Lines.Num() - 2;   // the header written at the end knows every stat
		LastRow = Lines.Num() - 3;
	}
	TArray<FString> Names;
	Lines[HeaderLine].ParseIntoArray(Names, TEXT(","), /*InCullEmpty=*/false);
	for (const FString& Name : Names)
	{
		Columns.Add(Name);
	}
	for (int32 Row = 1; Row <= LastRow; ++Row)
	{
		TArray<FString> Values;
		Lines[Row].ParseIntoArray(Values, TEXT(","), /*InCullEmpty=*/false);
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			// Rows written before a stat first appeared are shorter: the stat was 0 then.
			const bool bHas = Values.IsValidIndex(Index) && Values[Index].IsNumeric();
			Columns[Names[Index]].Add(bHas ? FCString::Atof(*Values[Index]) : 0.f);
		}
	}
	return Columns;
}

float HollowFrameStats::PercentileOf(const TConstArrayView<float> Values, const float Percent)
{
	if (Values.IsEmpty())
	{
		return 0.f;
	}
	TArray<float> Sorted(Values.GetData(), Values.Num());
	Sorted.Sort();
	const int32 Rank = FMath::CeilToInt(FMath::Clamp(Percent, 0.f, HollowFrameStatsPrivate::Hundred) / HollowFrameStatsPrivate::Hundred * Sorted.Num());
	return Sorted[FMath::Clamp(Rank - 1, 0, Sorted.Num() - 1)];
}

FHollowTimingSummary HollowFrameStats::Summarize(const TConstArrayView<float> Values, const float Percent)
{
	FHollowTimingSummary Summary;
	Summary.Count = Values.Num();
	if (Values.IsEmpty())
	{
		return Summary;
	}
	Summary.Average = static_cast<float>(Algo::Accumulate(Values, 0.0) / Values.Num());
	Summary.Median = PercentileOf(Values, HollowFrameStatsPrivate::Median);
	Summary.Percentile = PercentileOf(Values, Percent);
	Summary.Max = *Algo::MaxElement(Values);
	return Summary;
}

FHollowFrameReport HollowFrameStats::Evaluate(const TMap<FString, TArray<float>>& InColumns, const UHollowPerfSettings& Criteria, const FHollowAudioClock& Audio)
{
	using namespace HollowFrameStatsPrivate;
	FHollowFrameReport Report;
	const TArray<float>* Frames = Find(InColumns, Columns::Frame);
	if (!Frames)
	{
		return Report;
	}
	const float Percent = Criteria.FramePercentile;
	auto Timer = [&InColumns, Percent](const TCHAR* Name)
	{
		const TArray<float>* Column = Find(InColumns, Name);
		return Column ? Summarize(*Column, Percent) : FHollowTimingSummary();
	};
	Report.Frames = Frames->Num();
	Report.Seconds = Sum(Frames) / 1000.0;
	Report.Frame = Summarize(*Frames, Percent);
	Report.GameThread = Timer(Columns::GameThread);
	Report.RenderThread = Timer(Columns::RenderThread);
	Report.RhiThread = Timer(Columns::RhiThread);
	Report.Gpu = Timer(Columns::Gpu);
	Report.AverageFps = Report.Seconds > 0.0 ? static_cast<float>(Report.Frames / Report.Seconds) : 0.f;
	const float Target = Criteria.TargetFrameMs();
	Report.WithinTarget = static_cast<float>(Algo::CountIf(*Frames, [Target](const float Ms) { return Ms <= Target; })) / Report.Frames;
	Report.Hitches = static_cast<int32>(Algo::CountIf(*Frames, [&Criteria](const float Ms) { return Ms > Criteria.HitchMs; }));
	Report.bFramePass = Report.AverageFps >= Criteria.TargetFps && Report.Frame.Percentile <= Target;

	const double Buffers = Report.Seconds * Audio.BuffersPerSecond();
	const double AudioMs = SumOnAnyThread(InColumns, Columns::AudioCategory, Columns::AudioRender);
	Report.bHasAudio = Buffers > 0.0 && AudioMs > 0.0;
	if (Report.bHasAudio)
	{
		Report.AudioMsPerBuffer = static_cast<float>(AudioMs / Buffers);
		Report.MetaSoundMsPerBuffer = static_cast<float>(SumOnAnyThread(InColumns, Columns::MetaSoundCategory, Columns::MetaSound) / Buffers);
		Report.AudioLoad = static_cast<float>(AudioMs / (Report.Seconds * 1000.0));
		Report.bAudioPass = Report.AudioMsPerBuffer <= Criteria.AudioBufferBudgetMs;
	}
	return Report;
}

FString FHollowFrameReport::Describe(const UHollowPerfSettings& Criteria) const
{
	FString Text = FString::Printf(TEXT("%s: %.1f FPS average (target %.0f), P%.0f frame %.2f ms (target %.2f), %.0f %% of %d frames within target, %d hitch(es) over %.0f ms, worst %.1f ms; "
		"game %.2f / render %.2f / RHI %.2f / GPU %.2f ms (P%.0f)"), bFramePass ? TEXT("PASS") : TEXT("FAIL"), AverageFps, Criteria.TargetFps, Criteria.FramePercentile,
		Frame.Percentile, Criteria.TargetFrameMs(), WithinTarget * 100.f, Frames, Hitches, Criteria.HitchMs, Frame.Max, GameThread.Percentile, RenderThread.Percentile,
		RhiThread.Percentile, Gpu.Percentile, Criteria.FramePercentile);
	Text += bHasAudio ? FString::Printf(TEXT("; audio %s: %.3f ms per buffer (budget %.2f), MetaSounds %.3f, load %.1f %%"), bAudioPass ? TEXT("PASS") : TEXT("FAIL"),
		AudioMsPerBuffer, Criteria.AudioBufferBudgetMs, MetaSoundMsPerBuffer, AudioLoad * 100.f) : FString(TEXT("; no audio device"));
	return Text;
}
