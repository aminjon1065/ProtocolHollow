#include "AI/HollowEnvQueryAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "HollowEditorLog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowEnvQueryAuthoring)

namespace HollowEnvQueryAuthoringPrivate
{
	const TCHAR* const OptionsKey = TEXT("options");
	const TCHAR* const GeneratorKey = TEXT("generator");
	const TCHAR* const TestsKey = TEXT("tests");
	const TCHAR* const ClassKey = TEXT("class");
	const TCHAR* const PropertiesKey = TEXT("properties");

	/** One node of the spec: its class (a subclass of Base) and its property texts. */
	struct FNodeSpec
	{
		UClass* Class = nullptr;
		TArray<TPair<FString, FString>> Properties;
	};

	struct FOptionSpec
	{
		FNodeSpec Generator;
		TArray<FNodeSpec> Tests;
	};

	bool ParseNode(const TSharedPtr<FJsonObject>& Json, const UClass* Base, FNodeSpec& Out, FString& OutError)
	{
		FString ClassPath;
		if (!Json.IsValid() || !Json->TryGetStringField(ClassKey, ClassPath))
		{
			OutError = TEXT("A node without a class.");
			return false;
		}
		Out.Class = FindObject<UClass>(nullptr, *ClassPath);
		if (!Out.Class || !Out.Class->IsChildOf(Base) || Out.Class->HasAnyClassFlags(CLASS_Abstract))
		{
			OutError = FString::Printf(TEXT("%s is not a concrete %s."), *ClassPath, *Base->GetName());
			return false;
		}
		const TSharedPtr<FJsonObject>* Properties = nullptr;
		if (Json->TryGetObjectField(PropertiesKey, Properties))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Property : (*Properties)->Values)
			{
				FString Text;
				if (!Property.Value.IsValid() || !Property.Value->TryGetString(Text))
				{
					OutError = FString::Printf(TEXT("%s.%s: values are Unreal text strings."), *Out.Class->GetName(), *Property.Key);
					return false;
				}
				Out.Properties.Emplace(Property.Key, Text);
			}
			// JSON objects are unordered: a fixed order keeps the build deterministic.
			Out.Properties.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B) { return A.Key < B.Key; });
		}
		return true;
	}

	bool ParseSpec(const FString& Spec, TArray<FOptionSpec>& Out, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Spec), Root) || !Root.IsValid() || !Root->TryGetArrayField(OptionsKey, Options))
		{
			OutError = TEXT("The spec is not {\"options\": [...]}.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& OptionValue : *Options)
		{
			const TSharedPtr<FJsonObject>* Option = nullptr;
			const TSharedPtr<FJsonObject>* Generator = nullptr;
			if (!OptionValue.IsValid() || !OptionValue->TryGetObject(Option) || !(*Option)->TryGetObjectField(GeneratorKey, Generator))
			{
				OutError = TEXT("An option without a generator.");
				return false;
			}
			FOptionSpec& Parsed = Out.AddDefaulted_GetRef();
			if (!ParseNode(*Generator, UEnvQueryGenerator::StaticClass(), Parsed.Generator, OutError))
			{
				return false;
			}
			const TArray<TSharedPtr<FJsonValue>>* Tests = nullptr;
			if ((*Option)->TryGetArrayField(TestsKey, Tests))
			{
				for (const TSharedPtr<FJsonValue>& TestValue : *Tests)
				{
					const TSharedPtr<FJsonObject>* Test = nullptr;
					if (!TestValue.IsValid() || !TestValue->TryGetObject(Test) || !ParseNode(*Test, UEnvQueryTest::StaticClass(), Parsed.Tests.AddDefaulted_GetRef(), OutError))
					{
						OutError = OutError.IsEmpty() ? TEXT("A test is not an object.") : OutError;
						return false;
					}
				}
			}
		}
		return true;
	}

	FProperty* FindNodeProperty(const UClass* Class, const FString& Name)
	{
		return FindFProperty<FProperty>(Class, *Name);
	}

	bool ApplyProperties(UObject& Node, const FNodeSpec& Spec, FString& OutError)
	{
		for (const TPair<FString, FString>& Property : Spec.Properties)
		{
			FProperty* Found = FindNodeProperty(Node.GetClass(), Property.Key);
			if (!Found)
			{
				OutError = FString::Printf(TEXT("%s has no property %s."), *Node.GetClass()->GetName(), *Property.Key);
				return false;
			}
			if (!Found->ImportText_InContainer(*Property.Value, &Node, &Node, PPF_None))
			{
				OutError = FString::Printf(TEXT("%s.%s cannot take \"%s\"."), *Node.GetClass()->GetName(), *Property.Key, *Property.Value);
				return false;
			}
		}
		return true;
	}

	/** The node is of the spec's class and every listed property equals the spec's text read into it. */
	bool NodeMatches(const UObject* Node, const FNodeSpec& Spec)
	{
		if (!Node || Node->GetClass() != Spec.Class)
		{
			return false;
		}
		for (const TPair<FString, FString>& Property : Spec.Properties)
		{
			const FProperty* Found = FindNodeProperty(Spec.Class, Property.Key);
			if (!Found)
			{
				return false;
			}
			// Read the text into a scratch value of the property and compare like the editor does.
			void* Wanted = FMemory::Malloc(Found->GetSize(), Found->GetMinAlignment());
			Found->InitializeValue(Wanted);
			const bool bRead = Found->ImportText_Direct(*Property.Value, Wanted, nullptr, PPF_None) != nullptr;
			const bool bEqual = bRead && Found->Identical(Found->ContainerPtrToValuePtr<void>(Node), Wanted, PPF_None);
			Found->DestroyValue(Wanted);
			FMemory::Free(Wanted);
			if (!bEqual)
			{
				return false;
			}
		}
		return true;
	}
}

UEnvQuery* UHollowEnvQueryAuthoring::CreateEnvQuery(const FString& PackagePath, const FString& AssetName)
{
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName));
	UEnvQuery* Query = Package ? NewObject<UEnvQuery>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional) : nullptr;
	if (!Query)
	{
		UE_LOG(LogHollowEditor, Error, TEXT("Could not create EQS query %s/%s."), *PackagePath, *AssetName);
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Query);
	Query->MarkPackageDirty();
	return Query;
}

FString UHollowEnvQueryAuthoring::BuildEnvQuery(UEnvQuery* Query, const FString& Spec)
{
	using namespace HollowEnvQueryAuthoringPrivate;
	if (!Query)
	{
		return TEXT("No query.");
	}
	TArray<FOptionSpec> Options;
	FString Error;
	if (!ParseSpec(Spec, Options, Error))
	{
		return Error;
	}
	Query->Modify();
	TArray<TObjectPtr<UEnvQueryOption>>& Built = Query->GetOptionsMutable();
	Built.Reset();
	for (const FOptionSpec& OptionSpec : Options)
	{
		UEnvQueryOption* Option = NewObject<UEnvQueryOption>(Query, NAME_None, RF_Transactional);
		Option->Generator = NewObject<UEnvQueryGenerator>(Query, OptionSpec.Generator.Class, NAME_None, RF_Transactional);
		if (!ApplyProperties(*Option->Generator, OptionSpec.Generator, Error))
		{
			return Error;
		}
		for (int32 Index = 0; Index < OptionSpec.Tests.Num(); ++Index)
		{
			UEnvQueryTest* Test = NewObject<UEnvQueryTest>(Query, OptionSpec.Tests[Index].Class, NAME_None, RF_Transactional);
			if (!ApplyProperties(*Test, OptionSpec.Tests[Index], Error))
			{
				return Error;
			}
			Test->TestOrder = Index;   // what the EQS editor sets from the graph
			Option->Tests.Add(Test);
		}
		Built.Add(Option);
	}
#if WITH_EDITORONLY_DATA
	Query->EdGraph = nullptr;   // a stale graph would put its own nodes back; the editor rebuilds one from the options
#endif
	Query->MarkPackageDirty();
	UE_LOG(LogHollowEditor, Log, TEXT("Built %s: %d option(s)"), *Query->GetPathName(), Built.Num());
	return FString();
}

bool UHollowEnvQueryAuthoring::EnvQueryMatches(const UEnvQuery* Query, const FString& Spec)
{
	using namespace HollowEnvQueryAuthoringPrivate;
	TArray<FOptionSpec> Options;
	FString Error;
	if (!Query || !ParseSpec(Spec, Options, Error) || Query->GetOptions().Num() != Options.Num())
	{
		return false;
	}
	for (int32 OptionIndex = 0; OptionIndex < Options.Num(); ++OptionIndex)
	{
		const UEnvQueryOption* Option = Query->GetOptions()[OptionIndex];
		const FOptionSpec& OptionSpec = Options[OptionIndex];
		if (!Option || !NodeMatches(Option->Generator, OptionSpec.Generator) || Option->Tests.Num() != OptionSpec.Tests.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < OptionSpec.Tests.Num(); ++Index)
		{
			if (!NodeMatches(Option->Tests[Index], OptionSpec.Tests[Index]) || Option->Tests[Index]->TestOrder != Index)
			{
				return false;
			}
		}
	}
	return true;
}
