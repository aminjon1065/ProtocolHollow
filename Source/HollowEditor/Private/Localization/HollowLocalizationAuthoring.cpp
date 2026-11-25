#include "Localization/HollowLocalizationAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HollowEditorLog.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/TextKey.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowLocalizationAuthoring)

UStringTable* UHollowLocalizationAuthoring::FindOrCreateStringTable(const FString& PackagePath, const FString& Name)
{
	const FString PackageName = PackagePath / Name;
	if (UStringTable* Existing = LoadObject<UStringTable>(nullptr, *(PackageName + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		return Existing;
	}
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		UE_LOG(LogHollowEditor, Error, TEXT("%s is not a package name"), *PackageName);
		return nullptr;
	}
	UPackage* Package = CreatePackage(*PackageName);
	UStringTable* Table = NewObject<UStringTable>(Package, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Table);
	Package->MarkPackageDirty();
	UE_LOG(LogHollowEditor, Log, TEXT("Created string table %s"), *Table->GetPathName());
	return Table;
}

FText UHollowLocalizationAuthoring::TableText(UStringTable* Table, const FString& Key, const FString& SourceString)
{
	if (!Table || Key.IsEmpty())
	{
		return FText::GetEmpty();
	}
	const FStringTableRef Entries = Table->GetMutableStringTable();
	FString Current;
	if (!Entries->GetSourceString(FTextKey(Key), Current) || !Current.Equals(SourceString, ESearchCase::CaseSensitive))
	{
		Table->Modify();
		Entries->SetSourceString(FTextKey(Key), SourceString, /*InDevNotes=*/FString());
		Table->MarkPackageDirty();
	}
	return FText::FromStringTable(Table->GetStringTableId(), FTextKey(Key));
}

bool UHollowLocalizationAuthoring::GetTableEntry(const FText& Text, FString& OutTableId, FString& OutKey)
{
	FName TableId;
	FTextKey Key;
	if (!FTextInspector::GetTableIdAndKey(Text, TableId, Key))
	{
		OutTableId.Reset();
		OutKey.Reset();
		return false;
	}
	OutTableId = TableId.ToString();
	OutKey = Key.ToString();
	return true;
}
