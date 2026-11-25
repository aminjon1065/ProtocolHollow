#include "FX/HollowNiagaraAuthoring.h"

#include "HollowEditorLog.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowNiagaraAuthoring)

namespace HollowNiagaraAuthoringPrivate
{
	/** The scripts whose constants the stack edits: the system's (emitter spawn and update live there) and each emitter's. */
	/** The system's scripts and its emitters'; with bEnabledOnly, not those of a switched-off emitter (never compiled). */
	TArray<UNiagaraScript*> Scripts(UNiagaraSystem& System, const bool bEnabledOnly = false)
	{
		TArray<UNiagaraScript*> Found;
		Found.Add(System.GetSystemSpawnScript());
		Found.Add(System.GetSystemUpdateScript());
		for (const FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
		{
			if (bEnabledOnly && !Handle.GetIsEnabled())
			{
				continue;
			}
			if (const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
			{
				Data->GetScripts(Found, /*bCompilableOnly=*/false);
			}
		}
		Found.RemoveAll([](const UNiagaraScript* Script) { return Script == nullptr; });
		return Found;
	}

	FString ValueText(const FNiagaraParameterStore& Store, const FNiagaraVariable& Variable)
	{
		const FNiagaraTypeDefinition& Type = Variable.GetType();
		if (Type == FNiagaraTypeDefinition::GetFloatDef())
		{
			return FString::SanitizeFloat(Store.GetParameterValue<float>(Variable));
		}
		if (Type == FNiagaraTypeDefinition::GetIntDef())
		{
			return FString::FromInt(Store.GetParameterValue<int32>(Variable));
		}
		if (Type == FNiagaraTypeDefinition::GetBoolDef())
		{
			return Store.GetParameterValue<FNiagaraBool>(Variable).GetValue() ? TEXT("true") : TEXT("false");
		}
		if (Type == FNiagaraTypeDefinition::GetVec2Def())
		{
			return Store.GetParameterValue<FVector2f>(Variable).ToString();
		}
		if (Type == FNiagaraTypeDefinition::GetVec3Def() || Type == FNiagaraTypeDefinition::GetPositionDef())
		{
			return Store.GetParameterValue<FVector3f>(Variable).ToString();
		}
		if (Type == FNiagaraTypeDefinition::GetColorDef())
		{
			return Store.GetParameterValue<FLinearColor>(Variable).ToString();
		}
		return FString::Printf(TEXT("<%s>"), *Type.GetName());
	}

	bool WriteValue(FNiagaraParameterStore& Store, const FNiagaraVariable& Variable, const FString& Text)
	{
		const FNiagaraTypeDefinition& Type = Variable.GetType();
		if (Type == FNiagaraTypeDefinition::GetFloatDef())
		{
			return Store.SetParameterValue(static_cast<float>(FCString::Atod(*Text)), Variable);
		}
		if (Type == FNiagaraTypeDefinition::GetIntDef())
		{
			return Store.SetParameterValue(FCString::Atoi(*Text), Variable);
		}
		if (Type == FNiagaraTypeDefinition::GetBoolDef())
		{
			FNiagaraBool Value;
			Value.SetValue(Text.ToBool());
			return Store.SetParameterValue(Value, Variable);
		}
		if (Type == FNiagaraTypeDefinition::GetVec2Def())
		{
			FVector2f Value;
			return Value.InitFromString(Text) && Store.SetParameterValue(Value, Variable);
		}
		if (Type == FNiagaraTypeDefinition::GetVec3Def() || Type == FNiagaraTypeDefinition::GetPositionDef())
		{
			FVector3f Value;
			return Value.InitFromString(Text) && Store.SetParameterValue(Value, Variable);
		}
		if (Type == FNiagaraTypeDefinition::GetColorDef())
		{
			FLinearColor Value;
			return Value.InitFromString(Text) && Store.SetParameterValue(Value, Variable);
		}
		return false;
	}
}

bool UHollowNiagaraAuthoring::BuildSingleEmitterSystem(UNiagaraSystem* System, UNiagaraEmitter* Template, const FString& EmitterName, const TArray<UNiagaraScript*>& UpdateModules)
{
	if (!System || !Template || EmitterName.IsEmpty())
	{
		UE_LOG(LogHollowEditor, Error, TEXT("BuildSingleEmitterSystem: needs a system, an emitter template and a name."));
		return false;
	}
	TSet<FGuid> Old;
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		Old.Add(Handle.GetId());
	}
	System->RemoveEmitterHandlesById(Old);
	const FGuid Id = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid);
	FNiagaraEmitterHandle* Handle = System->GetEmitterHandles().FindByPredicate([&Id](const FNiagaraEmitterHandle& Candidate) { return Candidate.GetId() == Id; });
	FVersionedNiagaraEmitterData* Data = Handle ? Handle->GetEmitterData() : nullptr;
	UNiagaraScriptSource* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
	UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
	UNiagaraNodeOutput* Update = Graph ? Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleUpdateScript, Data->UpdateScriptProps.Script->GetUsageId()) : nullptr;
	if (!Update)
	{
		UE_LOG(LogHollowEditor, Error, TEXT("BuildSingleEmitterSystem: %s has no particle update stack."), *Template->GetName());
		return false;
	}
	Handle->SetName(FName(*EmitterName), *System);
	for (UNiagaraScript* Module : UpdateModules)
	{
		if (!Module || !FNiagaraStackGraphUtilities::AddScriptModuleToStack(Module, *Update))
		{
			UE_LOG(LogHollowEditor, Error, TEXT("BuildSingleEmitterSystem: could not add %s."), *GetNameSafe(Module));
			return false;
		}
	}
	return Compile(System);   // node layout is cosmetic (RelayoutGraph is not exported)
}

TArray<FString> UHollowNiagaraAuthoring::Describe(UNiagaraSystem* System)
{
	using namespace HollowNiagaraAuthoringPrivate;
	TArray<FString> Lines;
	if (!System)
	{
		return Lines;
	}
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		const UNiagaraScriptSource* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
		Lines.Add(FString::Printf(TEXT("emitter %s (%s)"), *Handle.GetName().ToString(), Data && Data->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU")));
		if (Source && Source->NodeGraph)
		{
			TArray<UNiagaraNodeFunctionCall*> Calls;
			Source->NodeGraph->GetNodesOfClass(Calls);
			for (const UNiagaraNodeFunctionCall* Call : Calls)
			{
				Lines.Add(FString::Printf(TEXT("module %s"), *Call->GetFunctionName()));
			}
		}
	}
	for (UNiagaraScript* Script : Scripts(*System))
	{
		TArray<FNiagaraVariable> Variables;
		Script->RapidIterationParameters.GetParameters(Variables);
		for (const FNiagaraVariable& Variable : Variables)
		{
			Lines.Add(FString::Printf(TEXT("%s = %s"), *Variable.GetName().ToString(), *ValueText(Script->RapidIterationParameters, Variable)));
		}
	}
	return Lines;
}

int32 UHollowNiagaraAuthoring::SetConstant(UNiagaraSystem* System, const FString& Suffix, const FString& Value)
{
	using namespace HollowNiagaraAuthoringPrivate;
	int32 Set = 0;
	if (!System || Suffix.IsEmpty())
	{
		return Set;
	}
	for (UNiagaraScript* Script : Scripts(*System))
	{
		TArray<FNiagaraVariable> Variables;
		Script->RapidIterationParameters.GetParameters(Variables);
		for (const FNiagaraVariable& Variable : Variables)
		{
			if (Variable.GetName().ToString().EndsWith(Suffix) && WriteValue(Script->RapidIterationParameters, Variable, Value))
			{
				Script->Modify();
				++Set;
			}
		}
	}
	System->MarkPackageDirty();
	return Set;
}

int32 UHollowNiagaraAuthoring::SetSpriteRenderer(UNiagaraSystem* System, UMaterialInterface* Material, const bool bVelocityAligned)
{
	int32 Set = 0;
	if (!System)
	{
		return Set;
	}
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		for (UNiagaraRendererProperties* Renderer : Data ? Data->GetRenderers() : TArray<UNiagaraRendererProperties*>())
		{
			if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Sprite->Modify();
				Sprite->Material = Material;
				Sprite->Alignment = bVelocityAligned ? ENiagaraSpriteAlignment::VelocityAligned : ENiagaraSpriteAlignment::Automatic;
				Sprite->FacingMode = bVelocityAligned ? ENiagaraSpriteFacingMode::FaceCameraPlane : ENiagaraSpriteFacingMode::Automatic;
				++Set;
			}
		}
	}
	System->MarkPackageDirty();
	return Set;
}

TArray<FString> UHollowNiagaraAuthoring::DescribeRenderers(UNiagaraSystem* System)
{
	TArray<FString> Lines;
	if (!System)
	{
		return Lines;
	}
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		Lines.Add(FString::Printf(TEXT("emitter %s %s"), *Handle.GetName().ToString(), Handle.GetIsEnabled() ? TEXT("on") : TEXT("off")));
		const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		for (const UNiagaraRendererProperties* Renderer : Data ? Data->GetRenderers() : TArray<UNiagaraRendererProperties*>())
		{
			Lines.Add(FString::Printf(TEXT("renderer %s %s %s"), *Handle.GetName().ToString(), *Renderer->GetClass()->GetName(), Renderer->GetIsEnabled() ? TEXT("on") : TEXT("off")));
		}
	}
	return Lines;
}

int32 UHollowNiagaraAuthoring::SetRendererEnabled(UNiagaraSystem* System, const FString& Emitter, const FString& RendererClass, const bool bEnabled)
{
	int32 Set = 0;
	if (!System)
	{
		return Set;
	}
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (!Emitter.IsEmpty() && Handle.GetName().ToString() != Emitter)
		{
			continue;
		}
		const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		for (UNiagaraRendererProperties* Renderer : Data ? Data->GetRenderers() : TArray<UNiagaraRendererProperties*>())
		{
			if (Renderer->GetClass()->GetName().Contains(RendererClass) && Renderer->GetIsEnabled() != bEnabled)
			{
				Renderer->Modify();
				Renderer->SetIsEnabled(bEnabled);
				++Set;
			}
		}
	}
	if (Set > 0)
	{
		System->MarkPackageDirty();
	}
	return Set;
}

int32 UHollowNiagaraAuthoring::SetRendererProperty(UNiagaraSystem* System, const FString& Emitter, const FString& RendererClass, const FString& PropertyName,
	const FString& Value)
{
	int32 Set = 0;
	if (!System)
	{
		return Set;
	}
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (!Emitter.IsEmpty() && Handle.GetName().ToString() != Emitter)
		{
			continue;
		}
		const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		for (UNiagaraRendererProperties* Renderer : Data ? Data->GetRenderers() : TArray<UNiagaraRendererProperties*>())
		{
			const FProperty* Property = Renderer && Renderer->GetClass()->GetName().Contains(RendererClass) ? Renderer->GetClass()->FindPropertyByName(FName(*PropertyName)) : nullptr;
			if (!Property)
			{
				continue;
			}
			void* Address = Property->ContainerPtrToValuePtr<void>(Renderer);
			FString Before;
			Property->ExportTextItem_Direct(Before, Address, nullptr, Renderer, PPF_None);
			if (Before == Value)
			{
				continue;
			}
			Renderer->Modify();
			if (Property->ImportText_Direct(*Value, Address, Renderer, PPF_None) == nullptr)
			{
				UE_LOG(LogHollowEditor, Error, TEXT("SetRendererProperty: %s of %s does not take '%s'."), *PropertyName, *Renderer->GetName(), *Value);
				continue;
			}
			Renderer->PostEditChange();   // the renderer's cached layout follows its properties
			++Set;
		}
	}
	if (Set > 0)
	{
		System->MarkPackageDirty();
	}
	return Set;
}

bool UHollowNiagaraAuthoring::SetEmitterOnGPU(UNiagaraSystem* System, const FString& Emitter, const bool bGPU)
{
	if (!System)
	{
		return false;
	}
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* Data = Handle.GetName().ToString() == Emitter ? Handle.GetEmitterData() : nullptr;
		if (!Data)
		{
			continue;
		}
		const ENiagaraSimTarget Wanted = bGPU ? ENiagaraSimTarget::GPUComputeSim : ENiagaraSimTarget::CPUSim;
		if (Data->SimTarget != Wanted)
		{
			const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
			Instance.Emitter->Modify();
			Data->SimTarget = Wanted;
			Instance.Emitter->MarkPackageDirty();
			System->MarkPackageDirty();
		}
		return true;
	}
	return false;
}

bool UHollowNiagaraAuthoring::SetEmitterEnabled(UNiagaraSystem* System, const FString& Emitter, const bool bEnabled)
{
	if (!System)
	{
		return false;
	}
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (Handle.GetName().ToString() == Emitter)
		{
			System->Modify();
			Handle.SetIsEnabled(bEnabled, *System, /*bRecompileIfChanged=*/false);
			System->MarkPackageDirty();
			return true;
		}
	}
	return false;
}

bool UHollowNiagaraAuthoring::Compile(UNiagaraSystem* System)
{
	using namespace HollowNiagaraAuthoringPrivate;
	if (!System)
	{
		return false;
	}
	System->RequestCompile(/*bForce=*/false);
	System->WaitForCompilationComplete(/*bIncludingGPUShaders=*/true, /*bShowProgress=*/false);
	bool bOk = true;
	for (UNiagaraScript* Script : Scripts(*System, /*bEnabledOnly=*/true))
	{
		if (Script->IsEmitterSpawnScript() || Script->IsEmitterUpdateScript())
		{
			continue;   // compiled as part of the system's own scripts, never on their own
		}
		const ENiagaraScriptCompileStatus Status = Script->GetLastCompileStatus();
		if (Status == ENiagaraScriptCompileStatus::NCS_Error || Status == ENiagaraScriptCompileStatus::NCS_Unknown)
		{
			UE_LOG(LogHollowEditor, Error, TEXT("Compile: %s of %s did not compile (%d)."), *Script->GetName(), *System->GetName(), static_cast<int32>(Status));
			bOk = false;
		}
	}
	return bOk;
}
