// Copyright Max Harris

#include "Selections/PCGSelectExByFilters.h"

#include "Async/ParallelFor.h"
#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExContext.h"
#include "Core/PCGExFilterTypeSets.h"
#include "Core/PCGExMTCommon.h"
#include "Core/PCGExPointFilter.h"
#include "Data/PCGDynamicMeshData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGPointArrayData.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/PCGUtilsDynMeshAttributeHelpers.h"
#include "Elements/PCGUtilsDynMeshSpaceHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataDomain.h"
#include "PCGContext.h"
#include "PCGExFilterCommon.h"
#include "PCGPin.h"
#include "Serialization/ArchiveCrc32.h"
#include "Target/PCGUtilsPainterSampling.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGSelectExByFilters"

namespace
{
	/** Releases a context the way PCG does, whether or not something still holds its handle. */
	struct FReleaseContext
	{
		void operator()(FPCGContext* InContext) const { FPCGContext::Release(InContext); }
	};

	/**
	 * PCGEx filters are initialized against a PCGEx context, and a Selector is evaluated inside whichever node
	 * consumes it, whose context is a plain FPCGContext. So each evaluation runs the filters in a short-lived PCGEx
	 * context that borrows the consumer's execution source and node: target-actor lookups resolve, and filter
	 * errors land on the node that evaluated the Selector.
	 */
	TUniquePtr<FPCGExContext, FReleaseContext> MakeFilterContext(const FPCGContext* Consumer)
	{
		TUniquePtr<FPCGExContext, FReleaseContext> FilterContext(new FPCGExContext());
		if (Consumer)
		{
			FilterContext->ExecutionSource = Consumer->ExecutionSource;
			FilterContext->Node = Consumer->Node;
		}
		return FilterContext;
	}

	class FSelectExByFiltersOperation final : public FPCGUtilsDynMeshSelectionOperation
	{
	public:
		explicit FSelectExByFiltersOperation(const UPCGSelectExByFiltersFactoryData* InFactory)
			: Factory(InFactory)
		{
		}

		virtual bool Initialize(const FPCGUtilsDynMeshSelectionEvaluationContext& InSelectionContext) override
		{
			using namespace UE::Geometry;

			if (!FPCGUtilsDynMeshSelectionOperation::Initialize(InSelectionContext) || !Factory)
			{
				return false;
			}

			const FDynamicMesh3& Mesh = InSelectionContext.Mesh;
			TArray<int32> VertexIDs;
			VertexIDs.Reserve(Mesh.VertexCount());
			for (const int32 VertexID : Mesh.VertexIndicesItr())
			{
				VertexIDs.Add(VertexID);
			}
			Passed.Init(false, Mesh.MaxVertexID());
			if (VertexIDs.IsEmpty())
			{
				return true;
			}

			// A Selector built without a consumer (tests, previews) has no target actor to place the mesh by.
			const FTransform MeshToPoints = Factory->bPositionsToWorld && Context
				? PCGUtilsDynMeshSpaceHelpers::ResolveMeshActorTransform(Context, InSelectionContext.MeshData, true)
				: FTransform::Identity;

			// Declared first so it is released last, after every PCGEx object that refers to it.
			const TUniquePtr<FPCGExContext, FReleaseContext> FilterContext = MakeFilterContext(Context);
			UPCGPointArrayData* Points = FilterContext->ManagedObjects->New<UPCGPointArrayData>();
			if (!Points)
			{
				PCGLog::LogErrorOnGraph(LOCTEXT("NoPoints", "SelectEx | By Filters could not create the vertex points it filters."), Context);
				return false;
			}
			FillPoints(*Points, Mesh, InSelectionContext.MeshData, VertexIDs, MeshToPoints);

			const TSharedRef<PCGExData::FPointIO> Source = MakeShared<PCGExData::FPointIO>(FilterContext->GetWeakSelfHandle(), Points);
			const TSharedRef<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(Source);
			Facade->bSupportsScopedGet = false;

			const TSharedRef<PCGExPointFilter::FManager> Manager = MakeShared<PCGExPointFilter::FManager>(Facade);
			if (!Manager->Init(FilterContext.Get(), Factory->Filters))
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("FiltersFailed",
					"SelectEx | By Filters: none of the {0} connected filters could initialize against the mesh's vertex points; the filters' own errors say why."),
					FText::AsNumber(Factory->Filters.Num())), Context);
				return false;
			}

			TArray<int8> Results;
			Results.Init(0, VertexIDs.Num());
			Manager->Test(PCGExMT::FScope(0, VertexIDs.Num()), Results, /*bParallel=*/true);
			for (int32 Index = 0; Index < VertexIDs.Num(); ++Index)
			{
				Passed[VertexIDs[Index]] = Results[Index] != 0;
			}
			return true;
		}

		virtual bool TestElement(int32 ElementID) const override
		{
			return Passed.IsValidIndex(ElementID) && Passed[ElementID];
		}

	private:
		/** One point per vertex, in VertexIDs order, carrying the mesh's @Data domain. */
		void FillPoints(UPCGPointArrayData& Points, const UE::Geometry::FDynamicMesh3& Mesh, const UPCGDynamicMeshData* MeshData,
			const TArray<int32>& VertexIDs, const FTransform& MeshToPoints) const
		{
			const FPCGMetadataDomain* SourceDomain = MeshData && MeshData->ConstMetadata()
				? MeshData->ConstMetadata()->GetConstMetadataDomain(PCGMetadataDomainID::Data) : nullptr;
			FPCGMetadataDomain* PointsDomain = Points.MutableMetadata()
				? Points.MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Data) : nullptr;
			if (SourceDomain && PointsDomain)
			{
				PointsDomain->Initialize(FPCGMetadataDomainInitializeParams(SourceDomain));
			}

			const UE::Geometry::FDynamicMeshColorOverlay* Colors = Mesh.HasAttributes() ? Mesh.Attributes()->PrimaryColors() : nullptr;
			// The Painter sample builder is the module's one definition of a vertex's world position and normal.
			const FPCGUtilsPainterSampleBuilder Samples(Mesh, MeshToPoints);

			Points.SetNumPoints(VertexIDs.Num());
			auto Transforms = Points.GetTransformValueRange();
			auto PointColors = Points.GetColorValueRange();
			auto Seeds = Points.GetSeedValueRange();
			ParallelFor(VertexIDs.Num(), [&](int32 Index)
			{
				const int32 VertexID = VertexIDs[Index];
				const FPCGUtilsDynMeshPainterSample Sample = Samples.MakeSample(VertexID);
				const FQuat Rotation = Factory->bRotationFromNormal
					? FRotationMatrix::MakeFromZ(Sample.WorldNormal).ToQuat() : FQuat::Identity;
				Transforms[Index] = FTransform(Rotation, Sample.WorldPosition);
				PointColors[Index] = Colors && Colors->ElementCount() > 0
					? FVector4(PCGUtilsDynMeshAttributeHelpers::GetVertexColor(Mesh, *Colors, VertexID, FVector4f::One()))
					: FVector4::One();
				Seeds[Index] = PCGHelpers::ComputeSeedFromPosition(Sample.WorldPosition);
			});
		}

		TObjectPtr<const UPCGSelectExByFiltersFactoryData> Factory;
		TBitArray<> Passed;
	};
}

TSharedPtr<FPCGUtilsDynMeshSelectionOperation> UPCGSelectExByFiltersFactoryData::CreateNativeOperationInternal() const
{
	return MakeShared<FSelectExByFiltersOperation>(this);
}

void UPCGSelectExByFiltersFactoryData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	// The filter definitions are inputs, so the provider records them as data dependencies of this factory.
	Super::AddToCrc(Ar, bFullDataCrc);
	if (bFullDataCrc)
	{
		bool bWorld = bPositionsToWorld;
		bool bNormal = bRotationFromNormal;
		Ar << bWorld;
		Ar << bNormal;
	}
}

#if WITH_EDITOR
FText UPCGSelectExByFiltersProviderSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "SelectEx | By Filters");
}

FText UPCGSelectExByFiltersProviderSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip",
		"Selects the vertices that pass the connected PCGEx point or data filters (ANDed, as in Uber Filter). Each "
		"vertex is filtered as a point: $Position (world space by default), $Color from vertex color, $Rotation Up "
		"along the normal, and the mesh's @Data domain. Read mesh attributes with an explicit @Data. selector.");
}
#endif

const FPCGDataTypeBaseId& UPCGSelectExByFiltersProviderSettings::GetFactoryTypeId() const
{
	return FPCGUtilsDynMeshSelectionFactoryDataTypeInfo::AsId();
}

TArray<FPCGPinProperties> UPCGSelectExByFiltersProviderSettings::SourceInputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGExFilters::Labels::SourceFiltersLabel, FPCGExDataTypeInfoFilter::AsId(), true, true).SetRequiredPin();
	return Pins;
}

UPCGUtilsDynMeshFactoryData* UPCGSelectExByFiltersProviderSettings::CreateFactory(
	FPCGContext* InContext, UPCGUtilsDynMeshFactoryData* InFactory) const
{
	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> Filters;
	const TArray<FPCGTaggedData> Inputs = InContext->InputData.GetInputsByPin(PCGExFilters::Labels::SourceFiltersLabel);
	for (int32 Index = 0; Index < Inputs.Num(); ++Index)
	{
		const UPCGExPointFilterFactoryData* Filter = Cast<const UPCGExPointFilterFactoryData>(Inputs[Index].Data);
		// Cluster (vtx/edge) filters need a cluster; a mesh's vertices are plain points.
		if (!Filter || !PCGExFactories::PointFilters().Contains(Filter->GetDataTypeId()))
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NotAPointFilter",
				"SelectEx | By Filters: Filters input {0} ({1}) is not a point or data filter; cluster filters cannot run on mesh vertices."),
				FText::AsNumber(Index), FText::FromString(GetNameSafe(Inputs[Index].Data ? Inputs[Index].Data->GetClass() : nullptr))),
				InContext);
			return nullptr;
		}
		Filters.Add(Filter);
	}
	if (Filters.IsEmpty())
	{
		PCGLog::LogErrorOnGraph(LOCTEXT("MissingFilters",
			"SelectEx | By Filters needs at least one PCGEx filter on its Filters pin."), InContext);
		return nullptr;
	}

	UPCGSelectExByFiltersFactoryData* Factory = InFactory
		? Cast<UPCGSelectExByFiltersFactoryData>(InFactory)
		: FPCGContext::NewObject_AnyThread<UPCGSelectExByFiltersFactoryData>(InContext);
	if (!Factory)
	{
		return nullptr;
	}

	Factory->Priority = Priority;
	Factory->Filters = MoveTemp(Filters);
	Factory->bPositionsToWorld = bPositionsToWorld;
	Factory->bRotationFromNormal = bRotationFromNormal;
	return Super::CreateFactory(InContext, Factory);
}

#undef LOCTEXT_NAMESPACE
