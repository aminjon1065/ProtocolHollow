#include "AI/HollowStateTreeAuthoring.h"

#include "AssetToolsModule.h"
#include "Brain/HollowStateTreeNodes.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Dom/JsonObject.h"
#include "GameplayTagContainer.h"
#include "HollowEditorLog.h"
#include "IAssetTools.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeFactory.h"
#include "StateTreeState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowStateTreeAuthoring)

namespace HollowStateTreeAuthoringPrivate
{
	const TCHAR* const NameKey = TEXT("name");
	const TCHAR* const ConditionsKey = TEXT("conditions");
	const TCHAR* const TasksKey = TEXT("tasks");
	const TCHAR* const TransitionsKey = TEXT("transitions");
	const TCHAR* const ChildrenKey = TEXT("children");
	const TCHAR* const TriggerKey = TEXT("trigger");
	const TCHAR* const EventKey = TEXT("event");
	const TCHAR* const TargetKey = TEXT("target");

	using FAddNode = void (*)(UStateTreeState&);

	template <typename T>
	void AddTask(UStateTreeState& State)
	{
		State.AddTask<T>();
	}

	template <typename T>
	void AddCondition(UStateTreeState& State)
	{
		State.AddEnterCondition<T>();
	}

	struct FNodeEntry
	{
		const UScriptStruct* Struct = nullptr;
		FAddNode Add = nullptr;
	};

	const TArray<FNodeEntry>& Tasks()
	{
		static const TArray<FNodeEntry> Entries =
		{
			{ FHollowSTTask_Patrol::StaticStruct(), &AddTask<FHollowSTTask_Patrol> },
			{ FHollowSTTask_Chase::StaticStruct(), &AddTask<FHollowSTTask_Chase> },
			{ FHollowSTTask_Investigate::StaticStruct(), &AddTask<FHollowSTTask_Investigate> },
			{ FHollowSTTask_Attack::StaticStruct(), &AddTask<FHollowSTTask_Attack> },
			{ FHollowSTTask_Breach::StaticStruct(), &AddTask<FHollowSTTask_Breach> },
			{ FHollowSTTask_Stunned::StaticStruct(), &AddTask<FHollowSTTask_Stunned> },
			{ FHollowSTTask_Track::StaticStruct(), &AddTask<FHollowSTTask_Track> },
			{ FHollowSTTask_Ambush::StaticStruct(), &AddTask<FHollowSTTask_Ambush> },
			{ FHollowSTTask_Retreat::StaticStruct(), &AddTask<FHollowSTTask_Retreat> },
		};
		return Entries;
	}

	const TArray<FNodeEntry>& Conditions()
	{
		static const TArray<FNodeEntry> Entries =
		{
			{ FHollowSTCond_HasTarget::StaticStruct(), &AddCondition<FHollowSTCond_HasTarget> },
			{ FHollowSTCond_TargetInEngageRange::StaticStruct(), &AddCondition<FHollowSTCond_TargetInEngageRange> },
			{ FHollowSTCond_HasStimulus::StaticStruct(), &AddCondition<FHollowSTCond_HasStimulus> },
			{ FHollowSTCond_HasObstacle::StaticStruct(), &AddCondition<FHollowSTCond_HasObstacle> },
			{ FHollowSTCond_IsStunned::StaticStruct(), &AddCondition<FHollowSTCond_IsStunned> },
			{ FHollowSTCond_IsHunting::StaticStruct(), &AddCondition<FHollowSTCond_IsHunting> },
			{ FHollowSTCond_IsRetreating::StaticStruct(), &AddCondition<FHollowSTCond_IsRetreating> },
		};
		return Entries;
	}

	struct FPendingTransition
	{
		UStateTreeState* From = nullptr;
		EStateTreeTransitionTrigger Trigger = EStateTreeTransitionTrigger::None;
		FGameplayTag Event;
		FString Target;
	};

	bool AddNodes(UStateTreeState& State, const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, const TArray<FNodeEntry>& Registry, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
		if (!Json->TryGetArrayField(Key, Names))
		{
			return true;
		}
		for (const TSharedPtr<FJsonValue>& Name : *Names)
		{
			const FString NodeName = Name->AsString();
			const FNodeEntry* Entry = Registry.FindByPredicate([&NodeName](const FNodeEntry& E) { return E.Struct->GetName() == NodeName; });
			if (!Entry)
			{
				OutError = FString::Printf(TEXT("state %s: unknown %s node %s"), *State.Name.ToString(), Key, *NodeName);
				return false;
			}
			Entry->Add(State);
		}
		return true;
	}

	bool BuildState(UStateTreeState& State, const TSharedPtr<FJsonObject>& Json, TArray<UStateTreeState*>& States, TArray<FPendingTransition>& Transitions, FString& OutError)
	{
		States.Add(&State);
		if (!AddNodes(State, Json, ConditionsKey, Conditions(), OutError) || !AddNodes(State, Json, TasksKey, Tasks(), OutError))
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* TransitionSpecs = nullptr;
		if (Json->TryGetArrayField(TransitionsKey, TransitionSpecs))
		{
			for (const TSharedPtr<FJsonValue>& Value : *TransitionSpecs)
			{
				const TSharedPtr<FJsonObject> Spec = Value->AsObject();
				FPendingTransition& Transition = Transitions.AddDefaulted_GetRef();
				Transition.From = &State;
				const int64 Trigger = StaticEnum<EStateTreeTransitionTrigger>()->GetValueByNameString(Spec->GetStringField(TriggerKey));
				if (Trigger == INDEX_NONE)
				{
					OutError = FString::Printf(TEXT("state %s: unknown trigger %s"), *State.Name.ToString(), *Spec->GetStringField(TriggerKey));
					return false;
				}
				Transition.Trigger = static_cast<EStateTreeTransitionTrigger>(Trigger);
				FString Event;
				if (Spec->TryGetStringField(EventKey, Event))
				{
					Transition.Event = FGameplayTag::RequestGameplayTag(FName(*Event), /*ErrorIfNotFound=*/false);
					if (!Transition.Event.IsValid())
					{
						OutError = FString::Printf(TEXT("state %s: unknown event tag %s"), *State.Name.ToString(), *Event);
						return false;
					}
				}
				Transition.Target = Spec->GetStringField(TargetKey);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
		if (Json->TryGetArrayField(ChildrenKey, Children))
		{
			for (const TSharedPtr<FJsonValue>& Child : *Children)
			{
				const TSharedPtr<FJsonObject> ChildJson = Child->AsObject();
				UStateTreeState& ChildState = State.AddChildState(FName(*ChildJson->GetStringField(NameKey)));
				if (!BuildState(ChildState, ChildJson, States, Transitions, OutError))
				{
					return false;
				}
			}
		}
		return true;
	}

	TArray<TSharedPtr<FJsonValue>> NodeNames(const TArray<FStateTreeEditorNode>& Nodes)
	{
		TArray<TSharedPtr<FJsonValue>> Names;
		for (const FStateTreeEditorNode& Node : Nodes)
		{
			const UScriptStruct* Struct = Node.Node.GetScriptStruct();
			Names.Add(MakeShared<FJsonValueString>(Struct ? Struct->GetName() : FString()));
		}
		return Names;
	}

	TSharedRef<FJsonObject> DescribeState(const UStateTreeState& State)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(NameKey, State.Name.ToString());
		Json->SetArrayField(ConditionsKey, NodeNames(State.EnterConditions));
		Json->SetArrayField(TasksKey, NodeNames(State.Tasks));
		TArray<TSharedPtr<FJsonValue>> Transitions;
		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			TSharedRef<FJsonObject> Spec = MakeShared<FJsonObject>();
			Spec->SetStringField(TriggerKey, StaticEnum<EStateTreeTransitionTrigger>()->GetNameStringByValue(static_cast<int64>(Transition.Trigger)));
			if (Transition.RequiredEvent.Tag.IsValid())
			{
				Spec->SetStringField(EventKey, Transition.RequiredEvent.Tag.ToString());
			}
			Spec->SetStringField(TargetKey, Transition.State.Name.ToString());
			Transitions.Add(MakeShared<FJsonValueObject>(Spec));
		}
		Json->SetArrayField(TransitionsKey, Transitions);
		TArray<TSharedPtr<FJsonValue>> Children;
		for (const UStateTreeState* Child : State.Children)
		{
			if (Child)
			{
				Children.Add(MakeShared<FJsonValueObject>(DescribeState(*Child)));
			}
		}
		Json->SetArrayField(ChildrenKey, Children);
		return Json;
	}
}

UStateTree* UHollowStateTreeAuthoring::CreateAIStateTree(const FString& PackagePath, const FString& AssetName)
{
	UStateTreeFactory* Factory = NewObject<UStateTreeFactory>();
	Factory->SetSchemaClass(UStateTreeAIComponentSchema::StaticClass());
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	UStateTree* StateTree = Cast<UStateTree>(AssetTools.CreateAsset(AssetName, PackagePath, UStateTree::StaticClass(), Factory));
	UE_CLOG(!StateTree, LogHollowEditor, Error, TEXT("Could not create StateTree %s/%s."), *PackagePath, *AssetName);
	return StateTree;
}

FString UHollowStateTreeAuthoring::BuildStateTree(UStateTree* StateTree, const FString& Spec)
{
	FString OutError;
	using namespace HollowStateTreeAuthoringPrivate;
	UStateTreeEditorData* EditorData = StateTree ? Cast<UStateTreeEditorData>(StateTree->EditorData) : nullptr;
	TSharedPtr<FJsonObject> Root;
	if (!EditorData || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Spec), Root) || !Root.IsValid())
	{
		OutError = TEXT("needs a StateTree asset with editor data and a JSON object spec");
		return OutError;
	}
	StateTree->Modify();
	EditorData->Modify();
	EditorData->SubTrees.Reset();
	UStateTreeState& RootState = EditorData->AddSubTree(FName(*Root->GetStringField(NameKey)));
	TArray<UStateTreeState*> States;
	TArray<FPendingTransition> Transitions;
	if (!BuildState(RootState, Root, States, Transitions, OutError))
	{
		return OutError;
	}
	for (const FPendingTransition& Transition : Transitions)
	{
		UStateTreeState* const* Target = States.FindByPredicate([&Transition](const UStateTreeState* State) { return State->Name.ToString() == Transition.Target; });
		if (!Target)
		{
			OutError = FString::Printf(TEXT("state %s: unknown transition target %s"), *Transition.From->Name.ToString(), *Transition.Target);
			return OutError;
		}
		if (Transition.Event.IsValid())
		{
			Transition.From->AddTransition(Transition.Trigger, Transition.Event, EStateTreeTransitionType::GotoState, *Target);
		}
		else
		{
			Transition.From->AddTransition(Transition.Trigger, EStateTreeTransitionType::GotoState, *Target);
		}
	}
	FStateTreeCompilerLog Log;
	if (!UStateTreeEditingSubsystem::CompileStateTree(StateTree, Log))
	{
		Log.DumpToLog(StateTree, LogHollowEditor);
		OutError = TEXT("the tree does not compile (see the log)");
		return OutError;
	}
	StateTree->MarkPackageDirty();
	return FString();
}

FString UHollowStateTreeAuthoring::DescribeStateTree(const UStateTree* StateTree)
{
	const UStateTreeEditorData* EditorData = StateTree ? Cast<UStateTreeEditorData>(StateTree->EditorData) : nullptr;
	if (!EditorData || EditorData->SubTrees.IsEmpty() || !EditorData->SubTrees[0])
	{
		return FString();
	}
	FString Text;
	FJsonSerializer::Serialize(HollowStateTreeAuthoringPrivate::DescribeState(*EditorData->SubTrees[0]), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
	return Text;
}
