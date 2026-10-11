// Copyright Max Harris

#include "Elements/PCGDynMeshExMerge.h"

#include "Core/PCGExMatchRuleFactoryProvider.h"
#include "Data/PCGDynamicMeshData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGPointArrayData.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMeshEditor.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Helpers/PCGExMatchingHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataDomain.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "UDynamicMesh.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGDynMeshExMerge"

// Named, not anonymous: unity builds put this file beside others with helpers of the same names.
namespace PCGDynMeshExMergePrivate
{
	using UE::Geometry::FDynamicMesh3;

	/**
	 * PCGEx match rules read their two sides as point data. A mesh has no points, so each mesh is stood in for by
	 * a one-point dataset that carries what a data-level rule can read: the mesh's tags and its @Data attributes.
	 * Nothing is written through it.
	 */
	TSharedPtr<PCGExData::FFacade> MakeStandIn(FPCGContext* Context, const UPCGDynamicMeshData* MeshData, const TSet<FString>& Tags, int32 Index)
	{
		UPCGPointArrayData* StandIn = FPCGContext::NewObject_AnyThread<UPCGPointArrayData>(Context);
		StandIn->SetNumPoints(1);

		const FPCGMetadataDomain* SourceDomain = MeshData->ConstMetadata()
			? MeshData->ConstMetadata()->GetConstMetadataDomain(PCGMetadataDomainID::Data) : nullptr;
		FPCGMetadataDomain* StandInDomain = StandIn->MutableMetadata()
			? StandIn->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Data) : nullptr;
		if (SourceDomain && StandInDomain)
		{
			StandInDomain->Initialize(FPCGMetadataDomainInitializeParams(SourceDomain));
		}

		const TSharedRef<PCGExData::FPointIO> PointIO = MakeShared<PCGExData::FPointIO>(Context->GetOrCreateHandle(), StandIn);
		PointIO->Tags = MakeShared<PCGExData::FTags>(Tags);
		PointIO->IOIndex = Index;
		return MakeShared<PCGExData::FFacade>(PointIO);
	}

	/** Appends a mesh to Combined, moving its material slots onto the shared list. */
	void AppendPiece(FDynamicMesh3& Combined, TArray<UMaterialInterface*>& Materials, const UPCGDynamicMeshData* PieceData)
	{
		FDynamicMesh3 Piece = PieceData->GetDynamicMesh()->GetMeshRef();
		if (!Piece.HasAttributes())
		{
			Piece.EnableAttributes();
		}
		if (!Piece.Attributes()->HasMaterialID())
		{
			Piece.Attributes()->EnableMaterialID();
		}
		const TArray<TObjectPtr<UMaterialInterface>>& OwnMaterials = PieceData->GetMaterials();
		UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIDs = Piece.Attributes()->GetMaterialID();
		TMap<int32, int32> SlotOfID;
		for (const int32 TriangleID : Piece.TriangleIndicesItr())
		{
			const int32 OwnID = MaterialIDs->GetValue(TriangleID);
			int32* Slot = SlotOfID.Find(OwnID);
			if (!Slot)
			{
				UMaterialInterface* Material = OwnMaterials.IsValidIndex(OwnID) ? OwnMaterials[OwnID].Get() : nullptr;
				const int32 Found = Materials.IndexOfByKey(Material);
				Slot = &SlotOfID.Add(OwnID, Found != INDEX_NONE ? Found : Materials.Add(Material));
			}
			MaterialIDs->SetValue(TriangleID, *Slot);
		}

		UE::Geometry::FMeshIndexMappings Mappings;
		UE::Geometry::FDynamicMeshEditor(&Combined).AppendMesh(&Piece, Mappings);
	}
}

#if WITH_EDITOR
FText UPCGDynMeshExMergeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "DynMeshEx | Merge");
}

FText UPCGDynMeshExMergeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Merges DynMeshes into groups chosen by PCGEx match rules, one DynMesh out per group, as the PCGEx Merge "
		"Points node does for points. A rule comparing an @Data attribute with itself leaves one mesh per value of "
		"that attribute. A mesh that matches no other passes through. The pieces are appended, not unioned: "
		"follow with DynMesh | Self Union for a clean solid.");
}
#endif

TArray<FPCGPinProperties> UPCGDynMeshExMergeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::DynamicMesh, true, true,
		LOCTEXT("InTooltip", "The DynMeshes to merge. Each is tested against the others with the Match Rules."))
		.SetRequiredPin();
	PCGExMatching::Helpers::DeclareMatchingRulesInputs(DataMatching, Pins);
	// PCGEx marks the rules pin Required while matching is on, and PCG does not run a node whose required pin is
	// empty. A rule that failed to build would then make this node, and everything after it, vanish without a
	// word. Left as an ordinary pin, the node runs and says that it has no rules.
	if (Pins.Last().PinStatus == EPCGPinStatus::Required)
	{
		Pins.Last().PinStatus = EPCGPinStatus::Normal;
	}
	return Pins;
}

TArray<FPCGPinProperties> UPCGDynMeshExMergeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::DynamicMesh, true, true,
		LOCTEXT("OutTooltip", "One DynMesh per group. A merged mesh has the @Data attributes of the first mesh in its group and the tags and materials of all of them. A mesh that matched no other is passed through unchanged."));
	return Pins;
}

FPCGElementPtr UPCGDynMeshExMergeSettings::CreateElement() const
{
	return MakeShared<FPCGDynMeshExMergeElement>();
}

bool FPCGDynMeshExMergeElement::ExecuteInternal(FPCGContext* Context) const
{
	using namespace PCGDynMeshExMergePrivate;
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDynMeshExMergeElement::ExecuteInternal);
	check(Context);

	const UPCGDynMeshExMergeSettings* Settings = Context->GetInputSettings<UPCGDynMeshExMergeSettings>();
	check(Settings);

	// The meshes that can be merged, in pin order. Anything else on the pin is reported and left out.
	TArray<FPCGTaggedData> Inputs;
	const TArray<FPCGTaggedData> PinInputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	for (int32 Index = 0; Index < PinInputs.Num(); ++Index)
	{
		const UPCGDynamicMeshData* MeshData = Cast<const UPCGDynamicMeshData>(PinInputs[Index].Data);
		if (!MeshData || !MeshData->GetDynamicMesh())
		{
			PCGLog::LogErrorOnGraph(FText::Format(
				LOCTEXT("NotDynMesh", "DynMeshEx | Merge: input {0} is not DynMesh data; skipped."), FText::AsNumber(Index)), Context);
			continue;
		}
		Inputs.Add(PinInputs[Index]);
	}
	if (Inputs.IsEmpty())
	{
		return true;
	}

	// Groups of positions in Inputs.
	TArray<TArray<int32>> Groups;
	if (!Settings->DataMatching.IsEnabled())
	{
		TArray<int32>& Everything = Groups.AddDefaulted_GetRef();
		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			Everything.Add(Index);
		}
	}
	else
	{
		TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> MatchRules;
		for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGExMatching::Labels::SourceMatchRulesLabel))
		{
			if (const UPCGExMatchRuleFactoryData* MatchRule = Cast<const UPCGExMatchRuleFactoryData>(Input.Data))
			{
				MatchRules.Add(MatchRule);
			}
		}
		if (MatchRules.IsEmpty())
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("NoMatchRules",
				"DynMeshEx | Merge has matching enabled but received no Match Rules. Connect a rule, or set Mode to Disabled to merge every mesh into one. Nothing was output."),
				Context);
			return true;
		}

		TArray<TSharedPtr<PCGExData::FFacade>> StandIns;
		StandIns.Reserve(Inputs.Num());
		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			StandIns.Add(MakeStandIn(Context, CastChecked<UPCGDynamicMeshData>(Inputs[Index].Data), Inputs[Index].Tags, Index));
		}

		const TSharedPtr<PCGExMatching::FDataMatcher> Matcher = MakeShared<PCGExMatching::FDataMatcher>();
		Matcher->SetDetails(&Settings->DataMatching);
		if (!Matcher->Init(MatchRules, StandIns, /*bThrowError=*/false))
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("MatcherInitFailed",
				"DynMeshEx | Merge could not prepare its Match Rules against the incoming meshes. Check that every tag or @Data attribute the rules read exists on the meshes. Nothing was output."),
				Context);
			return true;
		}
		PCGExMatching::Helpers::GetMatchingSourcePartitions(Matcher, StandIns, Groups, Settings->bExclusiveGroups, nullptr);
	}

	for (TArray<int32>& Group : Groups)
	{
		if (Group.IsEmpty())
		{
			continue;
		}
		// Pin order, so which mesh is first in a group, and so whose @Data the result keeps, does not depend on
		// the order the matcher found them in.
		Group.Sort();

		if (Group.Num() == 1)
		{
			FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef(Inputs[Group[0]]);
			Output.Pin = PCGPinConstants::DefaultOutputLabel;
			continue;
		}

		FDynamicMesh3 Combined;
		Combined.EnableAttributes();
		Combined.Attributes()->EnableMaterialID();
		TArray<UMaterialInterface*> Materials;
		TSet<FString> Tags;
		for (const int32 Index : Group)
		{
			AppendPiece(Combined, Materials, CastChecked<UPCGDynamicMeshData>(Inputs[Index].Data));
			Tags.Append(Inputs[Index].Tags);
		}

		// The result is the first mesh of the group with the others added to it: it starts from that mesh's
		// metadata (every domain, @Data included) and target actor.
		const FPCGTaggedData& First = Inputs[Group[0]];
		UPCGDynamicMeshData* OutputData = FPCGContext::NewObject_AnyThread<UPCGDynamicMeshData>(Context);
		OutputData->InitializeFromData(CastChecked<UPCGDynamicMeshData>(First.Data));
		OutputData->Initialize(MoveTemp(Combined), Materials);

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef(First);
		Output.Data = OutputData;
		Output.Tags = MoveTemp(Tags);
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
