#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "HAL/FileManager.h"
#include "Internationalization/InternationalizationArchive.h"
#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "LocTextHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"

namespace HollowLocalizationTests
{
	/** LOC-01 (ADR-0048): the Game localization target (Config/Localization/Game.ini, Build/Scripts/localize-mac.sh). */
	FString TargetDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Localization/Game"));
	}

	const TCHAR* const Russian = TEXT("ru");

	/** The argument names of a format pattern ({0}, {Name}; an escaped `{ is no argument). */
	TSet<FString> ArgumentsOf(const FString& Pattern)
	{
		TSet<FString> Names;
		for (int32 Index = 0; Index < Pattern.Len(); ++Index)
		{
			if (Pattern[Index] == TCHAR('`'))
			{
				++Index;   // the next character is literal
				continue;
			}
			if (Pattern[Index] != TCHAR('{'))
			{
				continue;
			}
			const int32 End = Pattern.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + 1);
			if (End == INDEX_NONE)
			{
				break;
			}
			Names.Add(Pattern.Mid(Index + 1, End - Index - 1));
			Index = End;
		}
		return Names;
	}

	/** Problems of a Russian plural form: every argument with |plural( needs one, few, many and other. */
	FString PluralProblem(const FString& Translation)
	{
		int32 From = 0;
		while ((From = Translation.Find(TEXT("|plural("), ESearchCase::CaseSensitive, ESearchDir::FromStart, From)) != INDEX_NONE)
		{
			const int32 End = Translation.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			const FString Forms = Translation.Mid(From, End == INDEX_NONE ? MAX_int32 : End - From);
			for (const TCHAR* Form : {TEXT("one="), TEXT("few="), TEXT("many="), TEXT("other=")})
			{
				if (!Forms.Contains(Form))
				{
					return FString::Printf(TEXT("plural without %s"), Form);
				}
			}
			From += 8;
		}
		return FString();
	}

	/** Whether the property's owner is the project's (its texts are the player's), or one of the engine's that a player reads. */
	bool IsPlayerTextProperty(const FProperty& Property)
	{
		const UStruct* Owner = Property.GetOwnerStruct();
		if (Owner && Owner->GetPackage()->GetName().StartsWith(TEXT("/Script/Hollow")))
		{
			return true;
		}
		return (Owner == UTextBlock::StaticClass() && Property.GetFName() == TEXT("Text"))
			|| (Owner == UWidget::StaticClass() && Property.GetFName() == TEXT("ToolTipText"));
	}

	/** "Namespace,Key" of every text the Game target gathered from the code (LOCTEXT), the namespaces without a package's. */
	TSet<FString> GatheredCodeTexts()
	{
		TSet<FString> Ids;
		FLocTextHelper Helper(TargetDir(), TEXT("Game.manifest"), TEXT("Game.archive"), TEXT("en"), {Russian}, nullptr);
		if (Helper.LoadAll(ELocTextHelperLoadFlags::Load))
		{
			Helper.EnumerateSourceTexts([&Ids](TSharedRef<FManifestEntry> Entry)
			{
				for (const FManifestContext& Context : Entry->Contexts)
				{
					Ids.Add(Entry->Namespace.GetString() + TEXT(",") + Context.Key.GetString());
				}
				return true;
			}, /*bCheckDependencies=*/false);
		}
		return Ids;
	}

	/** The gathered code texts, read once per run of the test. */
	const TSet<FString>* CodeTexts = nullptr;

	/** Whether Text is empty, an entry of a loaded string table that has it, or a code text (LOCTEXT) the gather found. */
	bool IsTableText(const FText& Text, FString& OutWhy)
	{
		if (Text.IsEmpty())
		{
			return true;
		}
		FName TableId;
		FTextKey Key;
		if (!FTextInspector::GetTableIdAndKey(Text, TableId, Key))
		{
			const TOptional<FString> Namespace = FTextInspector::GetNamespace(Text);
			const TOptional<FString> TextKey = FTextInspector::GetKey(Text);
			if (CodeTexts && Namespace && TextKey
				&& CodeTexts->Contains(TextNamespaceUtil::StripPackageNamespace(Namespace.GetValue()) + TEXT(",") + TextKey.GetValue()))
			{
				return true;   // a default from the code: localized under its LOCTEXT key
			}
			OutWhy = TEXT("a plain text, neither a string table entry nor a gathered code text");
			return false;
		}
		IStringTableEngineBridge::FullyLoadStringTableAsset(TableId);
		const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(TableId);
		if (!Table.IsValid() || !Table->FindEntry(Key).IsValid())
		{
			OutWhy = FString::Printf(TEXT("%s has no %s"), *TableId.ToString(), *FString(Key.ToString()));
			return false;
		}
		return true;
	}

	void CheckTexts(const UStruct& Type, const void* Data, const FString& Where, const bool bAllProperties, TArray<FString>& OutProblems);

	void CheckValue(const FProperty& Property, const void* Value, const FString& Where, TArray<FString>& OutProblems)
	{
		if (const FTextProperty* TextProperty = CastField<FTextProperty>(&Property))
		{
			const FText& Text = TextProperty->GetPropertyValue(Value);
			FString Why;
			if (!IsTableText(Text, Why))
			{
				OutProblems.Add(FString::Printf(TEXT("%s \"%s\": %s"), *Where, *Text.ToString(), *Why));
			}
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			CheckTexts(*StructProperty->Struct, Value, Where, /*bAllProperties=*/true, OutProblems);
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			FScriptArrayHelper Array(ArrayProperty, Value);
			for (int32 Index = 0; Index < Array.Num(); ++Index)
			{
				CheckValue(*ArrayProperty->Inner, Array.GetRawPtr(Index), FString::Printf(TEXT("%s[%d]"), *Where, Index), OutProblems);
			}
		}
	}

	/** Every text in Data (texts, structs, arrays; never through object references), checked to come from a string table. */
	void CheckTexts(const UStruct& Type, const void* Data, const FString& Where, const bool bAllProperties, TArray<FString>& OutProblems)
	{
		for (TFieldIterator<FProperty> It(&Type); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Transient) || (!bAllProperties && !IsPlayerTextProperty(**It)))
			{
				continue;
			}
			for (int32 Element = 0; Element < It->ArrayDim; ++Element)
			{
				CheckValue(**It, It->ContainerPtrToValuePtr<void>(Data, Element), Where + TEXT(".") + It->GetName(), OutProblems);
			}
		}
	}

	/** The widgets of a widget blueprint's tree, its own and those of the user widgets it holds. */
	void CheckWidgetTree(const UWidgetTree& Tree, const FString& Where, TArray<FString>& OutProblems)
	{
		Tree.ForEachWidget([&](UWidget* Widget)
		{
			CheckTexts(*Widget->GetClass(), Widget, Where + TEXT("/") + Widget->GetName(), /*bAllProperties=*/false, OutProblems);
		});
	}

	/** Whether Type holds a text anywhere (structs, arrays, maps, sets), for the network check. */
	bool HoldsText(const FProperty& Property, TSet<const UStruct*>& Seen)
	{
		if (Property.IsA<FTextProperty>())
		{
			return true;
		}
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			if (Seen.Contains(StructProperty->Struct))
			{
				return false;
			}
			Seen.Add(StructProperty->Struct);
			for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
			{
				if (HoldsText(**It, Seen))
				{
					return true;
				}
			}
			return false;
		}
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
		{
			return HoldsText(*ArrayProperty->Inner, Seen);
		}
		if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
		{
			return HoldsText(*SetProperty->ElementProp, Seen);
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
		{
			return HoldsText(*MapProperty->KeyProp, Seen) || HoldsText(*MapProperty->ValueProp, Seen);
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLocalizationCompleteTest, "Hollow.Editor.Localization.Complete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLocalizationCompleteTest::RunTest(const FString& Parameters)
{
	// LOC-01 (ADR-0048): every gathered player text has its Russian translation, none empty, with the same format
	// arguments as its English source and whole Russian plural forms; the compiled ru/Game.locres says the same.
	using namespace HollowLocalizationTests;
	FLocTextHelper Helper(TargetDir(), TEXT("Game.manifest"), TEXT("Game.archive"), TEXT("en"), {Russian}, nullptr);
	FText Error;
	if (!TestTrue(FString::Printf(TEXT("The Game target loads (%s)"), *TargetDir()), Helper.LoadAll(ELocTextHelperLoadFlags::Load, &Error)))
	{
		AddError(Error.ToString());
		return false;
	}
	FTextLocalizationResource Compiled;
	if (!TestTrue(TEXT("ru/Game.locres loads"), Compiled.LoadFromFile(TargetDir() / Russian / TEXT("Game.locres"), /*Priority=*/0)))
	{
		return false;
	}
	int32 Entries = 0;
	TArray<FString> Problems;
	Helper.EnumerateSourceTexts([&](TSharedRef<FManifestEntry> Entry)
	{
		const FString& Source = Entry->Source.Text;
		for (const FManifestContext& Context : Entry->Contexts)
		{
			++Entries;
			const FString Id = FString::Printf(TEXT("%s,%s \"%s\""), *Entry->Namespace.GetString(), *Context.Key.GetString(), *Source);
			if (Source.IsEmpty())
			{
				Problems.Add(Id + TEXT(": an empty source text"));
				continue;
			}
			const TSharedPtr<FArchiveEntry> Translated = Helper.FindTranslation(Russian, Entry->Namespace, Context.Key, Context.KeyMetadataObj);
			const FString Translation = Translated.IsValid() ? Translated->Translation.Text : FString();
			if (Translation.IsEmpty())
			{
				Problems.Add(Id + TEXT(": not translated"));
				continue;
			}
			if (Translated->Source.Text != Source)
			{
				Problems.Add(Id + FString::Printf(TEXT(": translated from an older source \"%s\""), *Translated->Source.Text));
			}
			if (ArgumentsOf(Source).Difference(ArgumentsOf(Translation)).Num() > 0 || ArgumentsOf(Translation).Difference(ArgumentsOf(Source)).Num() > 0)
			{
				Problems.Add(Id + FString::Printf(TEXT(": the arguments differ in \"%s\""), *Translation));
			}
			if (const FString Plural = PluralProblem(Translation); !Plural.IsEmpty())
			{
				Problems.Add(Id + TEXT(": ") + Plural);
			}
			const FTextLocalizationResource::FEntry* Shipped = Compiled.Entries.Find(FTextId(FTextKey(Entry->Namespace.GetString()), FTextKey(Context.Key.GetString())));
			if (!Shipped || !Shipped->LocalizedString.IsValid() || *Shipped->LocalizedString != Translation)
			{
				Problems.Add(Id + TEXT(": ru/Game.locres is not compiled from it (run Build/Scripts/localize-mac.sh)"));
			}
		}
		return true;
	}, /*bCheckDependencies=*/true);
	for (const FString& Problem : Problems)
	{
		AddError(Problem);
	}
	TestTrue(FString::Printf(TEXT("Player texts gathered (%d)"), Entries), Entries > 0);
	AddInfo(FString::Printf(TEXT("%d player texts, %d problems"), Entries, Problems.Num()));
	return Problems.IsEmpty();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLocalizationDataTextsTest, "Hollow.Editor.Localization.DataTexts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLocalizationDataTextsTest::RunTest(const FString& Parameters)
{
	// LOC-01 (ADR-0048): every text a player reads in the data (data assets, widget trees, blueprint defaults of the
	// project's classes) is a string table entry, so it is gathered under a stable key and translated.
	using namespace HollowLocalizationTests;
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	FARFilter Filter;
	for (const TCHAR* Root : {TEXT("/Game/Hollow"), TEXT("/HollowSector_Docks"), TEXT("/HollowSector_Cathedral")})
	{
		Filter.PackagePaths.Add(Root);
	}
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UDataAsset::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

	const TSet<FString> Gathered = GatheredCodeTexts();
	CodeTexts = &Gathered;
	ON_SCOPE_EXIT { CodeTexts = nullptr; };
	TestTrue(TEXT("The Game target's code texts are gathered"), Gathered.Num() > 50);
	TArray<FString> Problems;
	int32 Checked = 0;
	for (const FAssetData& Asset : Assets)
	{
		UObject* Object = Asset.GetAsset();
		const FString Where = Asset.AssetName.ToString();
		if (const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Object))
		{
			if (WidgetBlueprint->WidgetTree)
			{
				CheckWidgetTree(*WidgetBlueprint->WidgetTree, Where, Problems);
				++Checked;
			}
		}
		else if (const UBlueprint* Blueprint = Cast<UBlueprint>(Object))
		{
			if (const UClass* Generated = Blueprint->GeneratedClass)
			{
				CheckTexts(*Generated, Generated->GetDefaultObject(), Where, /*bAllProperties=*/false, Problems);
				++Checked;
			}
		}
		else if (Object)
		{
			CheckTexts(*Object->GetClass(), Object, Where, /*bAllProperties=*/false, Problems);
			++Checked;
		}
	}
	for (const FString& Problem : Problems)
	{
		AddError(Problem);
	}
	TestTrue(FString::Printf(TEXT("Assets checked (%d)"), Checked), Checked > 0);
	return Problems.IsEmpty();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLocalizationSourceAsciiTest, "Hollow.Editor.Localization.SourceAscii",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLocalizationSourceAsciiTest::RunTest(const FString& Parameters)
{
	// CLAUDE.md sec. 2 + LOC-01: code, configs and scripts are ASCII (Windows PowerShell 5.1 reads BOM-less files as ANSI);
	// a translation lives only in Content/Localization/Game, never as a literal in the code or the content scripts.
	const FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const TCHAR* const Folders[] = {TEXT("Source"), TEXT("Config"), TEXT("Build/Scripts"), TEXT("Build/Python"), TEXT("Build/Blender")};
	const TCHAR* const Extensions[] = {TEXT("h"), TEXT("cpp"), TEXT("inl"), TEXT("cs"), TEXT("ini"), TEXT("sh"), TEXT("ps1"), TEXT("psm1"), TEXT("cmd"), TEXT("py")};
	int32 Files = 0;
	TArray<FString> Problems;
	for (const TCHAR* Folder : Folders)
	{
		for (const TCHAR* Extension : Extensions)
		{
			TArray<FString> Found;
			IFileManager::Get().FindFilesRecursive(Found, *(Project / Folder), *FString::Printf(TEXT("*.%s"), Extension), /*Files=*/true, /*Directories=*/false);
			Found.Sort();
			for (const FString& File : Found)
			{
				TArray<uint8> Bytes;
				if (!FFileHelper::LoadFileToArray(Bytes, *File))
				{
					Problems.Add(File + TEXT(": unreadable"));
					continue;
				}
				++Files;
				int32 Line = 1;
				for (const uint8 Byte : Bytes)
				{
					if (Byte == '\n')
					{
						++Line;
					}
					else if (Byte >= 0x80)
					{
						Problems.Add(FString::Printf(TEXT("%s:%d: a non-ASCII byte 0x%02X"), *File.RightChop(Project.Len()), Line, Byte));
						break;
					}
				}
			}
		}
	}
	for (const FString& Problem : Problems)
	{
		AddError(Problem);
	}
	TestTrue(FString::Printf(TEXT("Files read (%d)"), Files), Files > 100);
	return Problems.IsEmpty();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLocalizationNetTextsTest, "Hollow.Editor.Localization.NetTexts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLocalizationNetTextsTest::RunTest(const FString& Parameters)
{
	// LOC-01 (ADR-0048): in co-op every machine shows its own language, so no text crosses the network: replicated
	// properties and RPC parameters of the project's classes carry ids and numbers, each machine resolves its texts.
	using namespace HollowLocalizationTests;
	TArray<FString> Problems;
	int32 Classes = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		const UClass* Class = *It;
		if (!Class->GetPackage()->GetName().StartsWith(TEXT("/Script/Hollow")) || Class->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			continue;
		}
		++Classes;
		for (TFieldIterator<FProperty> Prop(Class, EFieldIteratorFlags::ExcludeSuper); Prop; ++Prop)
		{
			TSet<const UStruct*> Seen;
			if (Prop->HasAnyPropertyFlags(CPF_Net) && HoldsText(**Prop, Seen))
			{
				Problems.Add(FString::Printf(TEXT("%s.%s replicates a text"), *Class->GetName(), *Prop->GetName()));
			}
		}
		for (TFieldIterator<UFunction> Function(Class, EFieldIteratorFlags::ExcludeSuper); Function; ++Function)
		{
			if (!Function->HasAnyFunctionFlags(FUNC_Net))
			{
				continue;
			}
			for (TFieldIterator<FProperty> Param(*Function); Param; ++Param)
			{
				TSet<const UStruct*> Seen;
				if (HoldsText(**Param, Seen))
				{
					Problems.Add(FString::Printf(TEXT("%s::%s sends a text (%s)"), *Class->GetName(), *Function->GetName(), *Param->GetName()));
				}
			}
		}
	}
	for (const FString& Problem : Problems)
	{
		AddError(Problem);
	}
	TestTrue(FString::Printf(TEXT("Project classes (%d)"), Classes), Classes > 50);
	return Problems.IsEmpty();
}

#endif // WITH_DEV_AUTOMATION_TESTS
