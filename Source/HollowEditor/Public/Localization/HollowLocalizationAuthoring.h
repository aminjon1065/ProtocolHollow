#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "HollowLocalizationAuthoring.generated.h"

class UStringTable;

/**
 * Editor-only helpers for localizable data texts (GP-01, ADR-0044): the content scripts put every new player-facing
 * text of the data (item and weapon names, labels) into a string table asset and give the data a text that refers to
 * it (LOCTABLE), so the gather of the Game target (RU + EN) finds it in one place. Editor Python cannot edit a string
 * table's entries, nor make a text that refers to one (unreal.Text is a culture-invariant string).
 */
UCLASS()
class HOLLOWEDITOR_API UHollowLocalizationAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The string table asset PackagePath/Name, created (and registered with the asset registry) when missing. Save it afterwards. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Localization")
	static UStringTable* FindOrCreateStringTable(const FString& PackagePath, const FString& Name);

	/** Sets Key to SourceString in Table if it differs (marking it dirty) and returns a text referring to that entry. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Localization")
	static FText TableText(UStringTable* Table, const FString& Key, const FString& SourceString);

	/** The string table and key Text refers to; false for any other text. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Localization")
	static bool GetTableEntry(const FText& Text, FString& OutTableId, FString& OutKey);
};
