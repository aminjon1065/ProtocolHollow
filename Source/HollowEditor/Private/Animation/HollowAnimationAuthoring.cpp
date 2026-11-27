#include "Animation/HollowAnimationAuthoring.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "HollowEditorLog.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAnimationAuthoring)

namespace HollowAnimationAuthoringPrivate
{
	/** The blend space's axes and samples are protected: the editor reaches them through reflection too. */
	FBlendParameter* Axis(UBlendSpace& BlendSpace, const int32 Index)
	{
		const FStructProperty* Property = CastField<FStructProperty>(UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters")));
		return Property && Index < Property->ArrayDim ? Property->ContainerPtrToValuePtr<FBlendParameter>(&BlendSpace, Index) : nullptr;
	}

	TArray<FBlendSample>* SampleData(UBlendSpace& BlendSpace)
	{
		const FArrayProperty* Property = CastField<FArrayProperty>(UBlendSpace::StaticClass()->FindPropertyByName(TEXT("SampleData")));
		return Property ? Property->ContainerPtrToValuePtr<TArray<FBlendSample>>(&BlendSpace) : nullptr;
	}
}

bool UHollowAnimationAuthoring::SetBlendSpace(UBlendSpace* BlendSpace, const TArray<FHollowBlendAxis>& Axes, const TArray<FHollowBlendSampleSpec>& Samples)
{
	using namespace HollowAnimationAuthoringPrivate;
	const int32 Dimensions = BlendSpace && BlendSpace->IsA<UBlendSpace1D>() ? 1 : 2;
	if (!BlendSpace || Axes.Num() != Dimensions || !SampleData(*BlendSpace))
	{
		UE_LOG(LogHollowEditor, Error, TEXT("SetBlendSpace: %s needs %d axis/axes"), *GetNameSafe(BlendSpace), Dimensions);
		return false;
	}
	BlendSpace->Modify();
	for (int32 Index = 0; Index < Axes.Num(); ++Index)
	{
		FBlendParameter* Parameter = Axis(*BlendSpace, Index);
		const FHollowBlendAxis& Wanted = Axes[Index];
		if (!Parameter || Wanted.Max <= Wanted.Min)
		{
			UE_LOG(LogHollowEditor, Error, TEXT("SetBlendSpace: %s axis %d (%s) is invalid"), *BlendSpace->GetName(), Index, *Wanted.Name);
			return false;
		}
		Parameter->DisplayName = Wanted.Name;
		Parameter->Min = Wanted.Min;
		Parameter->Max = Wanted.Max;
		Parameter->GridNum = FMath::Max(1, Wanted.GridDivisions);
		Parameter->bSnapToGrid = false;
		Parameter->bWrapInput = Wanted.bWrap;
	}
	while (BlendSpace->GetNumberOfBlendSamples() > 0)
	{
		BlendSpace->DeleteSample(0);
	}
	for (const FHollowBlendSampleSpec& Wanted : Samples)
	{
		const int32 Index = Wanted.Animation && Wanted.Animation->GetSkeleton() == BlendSpace->GetSkeleton()
			? BlendSpace->AddSample(Wanted.Animation, Wanted.Value) : INDEX_NONE;
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogHollowEditor, Error, TEXT("SetBlendSpace: %s cannot take %s at %s"), *BlendSpace->GetName(), *GetNameSafe(Wanted.Animation), *Wanted.Value.ToString());
			return false;
		}
		(*SampleData(*BlendSpace))[Index].RateScale = Wanted.RateScale;
	}
	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->PostEditChange();
	BlendSpace->MarkPackageDirty();
	return BlendSpace->GetNumberOfBlendSamples() == Samples.Num();
}

TArray<FString> UHollowAnimationAuthoring::DescribeBlendSpace(const UBlendSpace* BlendSpace)
{
	TArray<FString> Lines;
	if (!BlendSpace)
	{
		return Lines;
	}
	const int32 Dimensions = BlendSpace->IsA<UBlendSpace1D>() ? 1 : 2;
	for (int32 Index = 0; Index < Dimensions; ++Index)
	{
		const FBlendParameter& Parameter = BlendSpace->GetBlendParameter(Index);
		Lines.Add(FString::Printf(TEXT("axis %s %.0f..%.0f /%d%s"), *Parameter.DisplayName, Parameter.Min, Parameter.Max, Parameter.GridNum,
			Parameter.bWrapInput ? TEXT(" wrap") : TEXT("")));
	}
	for (int32 Index = 0; Index < BlendSpace->GetNumberOfBlendSamples(); ++Index)
	{
		const FBlendSample& Sample = BlendSpace->GetBlendSample(Index);
		Lines.Add(FString::Printf(TEXT("sample %s at (%.0f, %.0f) x%.2f"), *GetNameSafe(Sample.Animation), Sample.SampleValue.X, Sample.SampleValue.Y, Sample.RateScale));
	}
	return Lines;
}

bool UHollowAnimationAuthoring::SetMontage(UAnimMontage* Montage, const FName SlotName, const TArray<FHollowMontageSegment>& Segments,
	const TArray<FHollowMontageSection>& Sections, const float BlendInSeconds, const float BlendOutSeconds)
{
	if (!Montage || SlotName.IsNone() || Segments.IsEmpty())
	{
		return false;
	}
	Montage->Modify();
	Montage->SlotAnimTracks.Reset();
	FSlotAnimationTrack& Track = Montage->AddSlot(SlotName);
	Track.AnimTrack.AnimSegments.Reset();
	float Length = 0.f;
	for (const FHollowMontageSegment& Wanted : Segments)
	{
		const float Full = Wanted.Animation ? Wanted.Animation->GetPlayLength() : 0.f;
		const float End = Wanted.EndTime > 0.f ? FMath::Min(Wanted.EndTime, Full) : Full;
		const float Start = FMath::Clamp(Wanted.StartTime, 0.f, End);
		if (End - Start <= UE_KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogHollowEditor, Error, TEXT("SetMontage: %s would play nothing of %s (%.2f..%.2f)"), *Montage->GetName(), *GetNameSafe(Wanted.Animation), Start, End);
			return false;
		}
		FAnimSegment Segment;
		Segment.SetAnimReference(Wanted.Animation, /*bInitialize=*/true);
		Segment.AnimStartTime = Start;
		Segment.AnimEndTime = End;
		Segment.AnimPlayRate = 1.f;
		Segment.LoopingCount = 1;
		Segment.StartPos = Length;
		Track.AnimTrack.AnimSegments.Add(Segment);
		Length += End - Start;
	}
	Montage->SetCompositeLength(Length);

	Montage->CompositeSections.Reset();
	for (const FHollowMontageSection& Section : Sections)
	{
		if (Section.Name.IsNone() || Section.Time < 0.f || Section.Time > Length || Montage->AddAnimCompositeSection(Section.Name, Section.Time) == INDEX_NONE)
		{
			UE_LOG(LogHollowEditor, Error, TEXT("SetMontage: %s cannot have section %s at %.2f s (%.2f s long)"), *Montage->GetName(), *Section.Name.ToString(), Section.Time, Length);
			return false;
		}
	}
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		const FHollowMontageSection* Wanted = Sections.FindByPredicate([&Section](const FHollowMontageSection& Spec) { return Spec.Name == Section.SectionName; });
		Section.NextSectionName = Wanted ? Wanted->Next : NAME_None;
	}
	Montage->BlendIn.SetBlendTime(BlendInSeconds);
	Montage->BlendOut.SetBlendTime(BlendOutSeconds);
	Montage->bEnableAutoBlendOut = true;
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return Montage->CompositeSections.Num() == Sections.Num();
}

TArray<FString> UHollowAnimationAuthoring::DescribeMontage(const UAnimMontage* Montage)
{
	TArray<FString> Lines;
	if (!Montage)
	{
		return Lines;
	}
	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		Lines.Add(TEXT("slot ") + Track.SlotName.ToString());
		for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
		{
			Lines.Add(FString::Printf(TEXT("segment %s %.2f..%.2f"), *GetNameSafe(Segment.GetAnimReference().Get()), Segment.AnimStartTime, Segment.AnimEndTime));
		}
	}
	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		Lines.Add(FString::Printf(TEXT("section %s at %.2f -> %s"), *Section.SectionName.ToString(), Section.GetTime(), *Section.NextSectionName.ToString()));
	}
	return Lines;
}

namespace HollowAnimationAuthoringPrivate
{
	/** Calls Visit(LOD model, section) for every section of Mesh wearing one of Slots (the LOD's material map honoured). */
	template <typename TVisit>
	void ForEachSlotSection(const USkeletalMesh& Mesh, const TArray<FName>& Slots, TVisit&& Visit)
	{
		FSkeletalMeshModel* Model = Mesh.GetImportedModel();
		if (!Model)
		{
			return;
		}
		TSet<int32> Wanted;
		const TArray<FSkeletalMaterial>& Materials = Mesh.GetMaterials();
		for (int32 Index = 0; Index < Materials.Num(); ++Index)
		{
			if (Slots.Contains(Materials[Index].MaterialSlotName))
			{
				Wanted.Add(Index);
			}
		}
		for (int32 Lod = 0; Lod < Model->LODModels.Num(); ++Lod)
		{
			const FSkeletalMeshLODInfo* Info = Mesh.GetLODInfo(Lod);
			FSkeletalMeshLODModel& LodModel = Model->LODModels[Lod];
			for (int32 Index = 0; Index < LodModel.Sections.Num(); ++Index)
			{
				FSkelMeshSection& Section = LodModel.Sections[Index];
				const int32 Material = Info && Info->LODMaterialMap.IsValidIndex(Index) && Info->LODMaterialMap[Index] != INDEX_NONE
					? Info->LODMaterialMap[Index] : Section.MaterialIndex;
				if (Wanted.Contains(Material))
				{
					Visit(LodModel, Section);
				}
			}
		}
	}
}

int32 UHollowAnimationAuthoring::DisableMeshSlots(USkeletalMesh* Mesh, const TArray<FName>& Slots)
{
	if (!Mesh || Slots.IsEmpty())
	{
		return 0;
	}
	int32 Disabled = 0;
	Mesh->Modify();
	HollowAnimationAuthoringPrivate::ForEachSlotSection(*Mesh, Slots, [&Disabled](FSkeletalMeshLODModel& LodModel, FSkelMeshSection& Section)
	{
		FSkelMeshSourceSectionUserData& User = FSkelMeshSourceSectionUserData::GetSourceSectionUserData(LodModel.UserSectionsData, Section);
		if (!Section.bDisabled || !User.bDisabled)
		{
			Section.bDisabled = true;
			User.bDisabled = true;
			++Disabled;
		}
	});
	if (Disabled > 0)
	{
		Mesh->PostEditChange();   // rebuilds its render data without them
		Mesh->MarkPackageDirty();
	}
	return Disabled;
}

FIntPoint UHollowAnimationAuthoring::CountDisabledSections(const USkeletalMesh* Mesh, const TArray<FName>& Slots)
{
	FIntPoint Count = FIntPoint::ZeroValue;
	if (Mesh)
	{
		HollowAnimationAuthoringPrivate::ForEachSlotSection(*Mesh, Slots, [&Count](FSkeletalMeshLODModel&, FSkelMeshSection& Section)
		{
			Count.X += Section.bDisabled ? 1 : 0;
			++Count.Y;
		});
	}
	return Count;
}

bool UHollowAnimationAuthoring::SetSkeletonSocket(USkeleton* Skeleton, const FName SocketName, const FName Bone, const FTransform& Relative)
{
	if (!Skeleton || SocketName.IsNone() || Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone) == INDEX_NONE)
	{
		UE_LOG(LogHollowEditor, Error, TEXT("SetSkeletonSocket: %s has no bone %s for %s"), *GetNameSafe(Skeleton), *Bone.ToString(), *SocketName.ToString());
		return false;
	}
	Skeleton->Modify();
	USkeletalMeshSocket* Socket = Skeleton->FindSocket(SocketName);
	if (!Socket)
	{
		Socket = NewObject<USkeletalMeshSocket>(Skeleton);
		Socket->SocketName = SocketName;
		Skeleton->Sockets.Add(Socket);
	}
	Socket->BoneName = Bone;
	Socket->RelativeLocation = Relative.GetLocation();
	Socket->RelativeRotation = Relative.Rotator();
	Socket->RelativeScale = Relative.GetScale3D();
	Skeleton->MarkPackageDirty();
	return true;
}

bool UHollowAnimationAuthoring::EnsureSkeletonSlot(USkeleton* Skeleton, const FName SlotName)
{
	if (!Skeleton || SlotName.IsNone())
	{
		return false;
	}
	if (!Skeleton->ContainsSlotName(SlotName))
	{
		Skeleton->Modify();
		Skeleton->RegisterSlotNode(SlotName);
		Skeleton->MarkPackageDirty();
	}
	return Skeleton->ContainsSlotName(SlotName);
}
