#include "Tests/HollowLocalizationTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Foundation/HollowButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/SlateRenderer.h"

namespace HollowLocalizationTests
{
	constexpr float FitTolerance = 1.0f;   // slate units: rounding of the measured width

	bool HasCyrillic(const FString& Text)
	{
		for (const TCHAR Character : Text)
		{
			if (Character >= 0x0400 && Character <= 0x04FF)
			{
				return true;
			}
		}
		return false;
	}

	/** Each widget of Widget's tree and of the user widgets in it, with its path. */
	void ForEachWidget(const UUserWidget& Widget, const FString& Path, const TFunctionRef<void(UWidget&, const FString&)> Visit)
	{
		if (!Widget.WidgetTree)
		{
			return;
		}
		Widget.WidgetTree->ForEachWidget([&](UWidget* Each)
		{
			const FString Here = Path + TEXT("/") + Each->GetName();
			Visit(*Each, Here);
			if (const UUserWidget* Nested = Cast<UUserWidget>(Each))
			{
				ForEachWidget(*Nested, Here, Visit);
			}
		});
	}

	TMap<FString, FString> ReadTexts(const UUserWidget& Widget)
	{
		TMap<FString, FString> Texts;
		ForEachWidget(Widget, Widget.GetName(), [&Texts](UWidget& Each, const FString& Path)
		{
			FText Text;
			if (const UTextBlock* Block = Cast<UTextBlock>(&Each))
			{
				Text = Block->GetText();
			}
			else if (const UHollowButton* Button = Cast<UHollowButton>(&Each))
			{
				Text = Button->GetLabel();
			}
			if (!Text.IsEmpty())
			{
				Texts.Add(Path, Text.ToString());
			}
		});
		return Texts;
	}

	/** The room every visible Slate widget under Widget gets, by laying the children out like a paint would. */
	void Arrange(const TSharedRef<SWidget>& Widget, const FGeometry& Geometry, TMap<const SWidget*, FVector2D>& OutRoom, TSet<const SWidget*>& OutScaled, const bool bScaled)
	{
		OutRoom.Add(&Widget.Get(), Geometry.GetLocalSize());
		if (bScaled)
		{
			OutScaled.Add(&Widget.Get());
		}
		const bool bScaleBox = Widget->GetType() == TEXT("SScaleBox");
		FArrangedChildren Children(EVisibility::Visible);
		Widget->ArrangeChildren(Geometry, Children);
		for (int32 Index = 0; Index < Children.Num(); ++Index)
		{
			Arrange(Children[Index].Widget, Children[Index].Geometry, OutRoom, OutScaled, bScaled || bScaleBox);
		}
	}

	TArray<FString> FindOverflows(UUserWidget& Widget, const FVector2D& ViewSize)
	{
		const TSharedRef<SWidget> Root = Widget.TakeWidget();
		Root->SlatePrepass(1.0f);
		TMap<const SWidget*, FVector2D> Room;
		TSet<const SWidget*> Scaled;
		Arrange(Root, FGeometry::MakeRoot(ViewSize, FSlateLayoutTransform()), Room, Scaled, /*bScaled=*/false);
		TArray<FString> Overflows;
		ForEachWidget(Widget, Widget.GetName(), [&](UWidget& Each, const FString& Path)
		{
			const UTextBlock* Block = Cast<UTextBlock>(&Each);
			const TSharedPtr<SWidget> Slate = Block ? Block->GetCachedWidget() : nullptr;
			const FVector2D* Given = Slate.IsValid() ? Room.Find(Slate.Get()) : nullptr;
			if (!Given || Block->GetAutoWrapText() || Scaled.Contains(Slate.Get()))
			{
				return;   // collapsed, wrapping or shrunk to fit
			}
			const float Wanted = Slate->GetDesiredSize().X;
			if (Wanted > Given->X + FitTolerance)
			{
				Overflows.Add(FString::Printf(TEXT("%s: \"%s\" (%.0f > %.0f)"), *Path, *Block->GetText().ToString(), Wanted, Given->X));
			}
		});
		return Overflows;
	}

	TArray<FString> Differences(const TMap<FString, FString>& Before, const TMap<FString, FString>& After)
	{
		TArray<FString> Out;
		for (const TPair<FString, FString>& Entry : Before)
		{
			const FString* Now = After.Find(Entry.Key);
			if (!Now || *Now != Entry.Value)
			{
				Out.Add(FString::Printf(TEXT("%s: \"%s\" -> \"%s\""), *Entry.Key, *Entry.Value, Now ? **Now : TEXT("(gone)")));
			}
		}
		for (const TPair<FString, FString>& Entry : After)
		{
			if (!Before.Contains(Entry.Key))
			{
				Out.Add(FString::Printf(TEXT("%s: (new) \"%s\""), *Entry.Key, *Entry.Value));
			}
		}
		return Out;
	}

	bool CanMeasureText()
	{
		return FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer() != nullptr;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
