#include "ViewModels/HollowInputGlyphSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowInputGlyphSet)

FText UHollowInputGlyphSet::GetLabel(const FKey& Key) const
{
	const FHollowKeyGlyph* Glyph = Glyphs.FindByPredicate([&Key](const FHollowKeyGlyph& Candidate) { return Candidate.Key == Key; });
	return Glyph ? Glyph->Label : Key.GetDisplayName(/*bLongDisplayName=*/false);
}

FKey UHollowInputGlyphSet::PickKey(const TConstArrayView<FKey> Keys, const ECommonInputType InputType)
{
	for (const FKey& Key : Keys)
	{
		const bool bGamepadKey = Key.IsGamepadKey();
		if ((InputType == ECommonInputType::Gamepad && bGamepadKey) || (InputType == ECommonInputType::MouseAndKeyboard && !bGamepadKey && !Key.IsTouch()))
		{
			return Key;
		}
	}
	return FKey();
}
