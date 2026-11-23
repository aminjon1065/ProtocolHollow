#include "Visor/HollowControlsHintSet.h"

#include "InputAction.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowControlsHintSet)

#define LOCTEXT_NAMESPACE "HollowControlsHint"

const FText& UHollowControlsHintSet::LabelFor(const FHollowControlsHintRow& Row, const FGameplayTag& ClassTag)
{
	const FHollowControlsClassLabel* Own = ClassTag.IsValid()
		? Row.ClassLabels.FindByPredicate([&ClassTag](const FHollowControlsClassLabel& Label) { return Label.ClassTag == ClassTag; })
		: nullptr;
	return Own ? Own->Label : Row.Label;
}

#if WITH_EDITOR
EDataValidationResult UHollowControlsHintSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (Rows[Index].Action.IsNull() || Rows[Index].Label.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("Row", "{0}: row {1} needs an input action and a label."), FText::FromName(GetFName()), Index));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

FText HollowControlsHintRules::Lines(const TConstArrayView<TPair<FText, FText>> KeysAndLabels)
{
	int32 KeyWidth = 0;
	for (const TPair<FText, FText>& Line : KeysAndLabels)
	{
		KeyWidth = FMath::Max(KeyWidth, Line.Key.ToString().Len());
	}
	TArray<FString> Lines;
	for (const TPair<FText, FText>& Line : KeysAndLabels)
	{
		const FString Key = Line.Key.ToString();
		Lines.Add(Key + FString::ChrN(KeyWidth - Key.Len() + ColumnGap, TEXT(' ')) + Line.Value.ToString());
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

#undef LOCTEXT_NAMESPACE
