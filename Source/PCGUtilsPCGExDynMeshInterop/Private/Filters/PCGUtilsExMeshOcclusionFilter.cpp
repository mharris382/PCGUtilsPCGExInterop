// Copyright Max Harris

#include "Filters/PCGUtilsExMeshOcclusionFilter.h"

#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExContext.h"
#include "Data/PCGDynamicMeshData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointElements.h"
#include "Details/PCGExSettingsDetails.h"
#include "Elements/PCGUtilsDynMeshSpaceHelpers.h"
#include "Geometry/PCGUtilsDynMeshOcclusion.h"
#include "PCGPin.h"
#include "UDynamicMesh.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGUtilsExMeshOcclusionFilter"

bool UPCGUtilsExMeshOcclusionFilterFactory::SupportsProxyEvaluation() const
{
	return Config.Direction.Input == EPCGExInputValueType::Constant && Config.MaxDistance.Input == EPCGExInputValueType::Constant;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGUtilsExMeshOcclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGUtilsExMeshOcclusionFilter::FFilter>(this);
}

void UPCGUtilsExMeshOcclusionFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	Config.Direction.RegisterBufferDependencies(InContext, FacadePreloader);
	Config.MaxDistance.RegisterBufferDependencies(InContext, FacadePreloader);
}

PCGExFactories::EPreparationResult UPCGUtilsExMeshOcclusionFilterFactory::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
{
	OccluderMeshes.Reset();
	for (const FPCGTaggedData& Input : InContext->InputData.GetInputsByPin(PCGUtilsExMeshOcclusionFilterConstants::OccludersPin))
	{
		if (const UPCGDynamicMeshData* MeshData = Cast<const UPCGDynamicMeshData>(Input.Data))
		{
			OccluderMeshes.Add(MeshData);
		}
	}

	Occluders = MakeShared<PCGUtilsDynMeshOcclusion::FOccluderSet>();
	for (int32 Index = 0; Index < OccluderMeshes.Num(); ++Index)
	{
		const UPCGDynamicMeshData* MeshData = OccluderMeshes[Index];
		const UE::Geometry::FDynamicMesh3* Mesh = MeshData->GetDynamicMesh() ? MeshData->GetDynamicMesh()->GetMeshPtr() : nullptr;
		const FTransform ToWorld = PCGUtilsDynMeshSpaceHelpers::ResolveMeshActorTransform(InContext, MeshData, Config.bOccludersAreActorLocal);
		if (!Mesh || Occluders->Add(*Mesh, ToWorld) == INDEX_NONE)
		{
			PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("EmptyOccluder",
				"Filter : Mesh Occlusion: Occluders input {0} has no triangles and blocks nothing; it is ignored."),
				FText::AsNumber(Index)), InContext);
		}
	}

	if (Occluders->IsEmpty())
	{
		if (MissingDataPolicy == EPCGExFilterNoDataFallback::Error)
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("NoOccluders",
				"Filter : Mesh Occlusion needs at least one DynMesh with triangles on its Occluders pin."), InContext);
		}
		return PCGExFactories::EPreparationResult::MissingData;
	}
	return Super::Prepare(InContext, TaskManager);
}

bool PCGUtilsExMeshOcclusionFilter::FFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade))
	{
		return false;
	}
	if (!TypedFactory->Occluders)
	{
		return false;
	}

	DirectionGetter = TypedFactory->Config.Direction.GetValueSetting(PCGEX_QUIET_HANDLING);
	DirectionGetter->bRegisterConsumable &= TypedFactory->bCleanupConsumableAttributes;
	if (!DirectionGetter->Init(InPointDataFacade))
	{
		return false;
	}
	MaxDistanceGetter = TypedFactory->Config.MaxDistance.GetValueSetting(PCGEX_QUIET_HANDLING);
	MaxDistanceGetter->bRegisterConsumable &= TypedFactory->bCleanupConsumableAttributes;
	if (!MaxDistanceGetter->Init(InPointDataFacade))
	{
		return false;
	}
	InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();
	return true;
}

bool PCGUtilsExMeshOcclusionFilter::FFilter::Test(const int32 PointIndex) const
{
	return TestAt(InTransforms[PointIndex], DirectionGetter->Read(PointIndex), MaxDistanceGetter->Read(PointIndex));
}

bool PCGUtilsExMeshOcclusionFilter::FFilter::Test(const PCGExData::FProxyPoint& Point) const
{
	return TestAt(Point.GetTransform(), TypedFactory->Config.Direction.Constant, TypedFactory->Config.MaxDistance.Constant);
}

bool PCGUtilsExMeshOcclusionFilter::FFilter::TestAt(const FTransform& PointTransform, FVector RayDirection, double MaxDistance) const
{
	const FPCGUtilsExMeshOcclusionFilterConfig& Config = TypedFactory->Config;
	if (Config.Direction.bFlip)
	{
		RayDirection = -RayDirection;
	}
	if (Config.bDirectionInPointSpace)
	{
		RayDirection = PointTransform.TransformVectorNoScale(RayDirection);
	}
	if (!RayDirection.Normalize())
	{
		// No direction to trace along: the point cannot be judged, so it fails either way.
		return false;
	}
	const FVector3d Origin = PointTransform.GetLocation() + RayDirection * FMath::Max(0.0, Config.OriginOffset);
	const bool bOccluded = TypedFactory->Occluders->IsOccluded(Origin, RayDirection, FMath::Abs(MaxDistance));
	return bOccluded == (Config.Result == EPCGDynMeshSelfOcclusionResult::Occluded);
}

TArray<FPCGPinProperties> UPCGUtilsExMeshOcclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins = Super::InputPinProperties();
	FPCGPinProperties& Pin = Pins.Emplace_GetRef(PCGUtilsExMeshOcclusionFilterConstants::OccludersPin, EPCGDataType::DynamicMesh, true, true);
	Pin.SetRequiredPin();
#if WITH_EDITOR
	Pin.Tooltip = LOCTEXT("OccludersTip", "Required. The DynMeshes that block rays. Several act together.");
#endif
	return Pins;
}

UPCGExFactoryData* UPCGUtilsExMeshOcclusionFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGUtilsExMeshOcclusionFilterFactory* NewFactory = InContext->ManagedObjects->New<UPCGUtilsExMeshOcclusionFilterFactory>();
	NewFactory->InitializationFailurePolicy = InitializationFailurePolicy;
	NewFactory->MissingDataPolicy = MissingDataPolicy;
	NewFactory->Config = Config;
	Super::CreateFactory(InContext, NewFactory);
	if (!NewFactory->Init(InContext))
	{
		InContext->ManagedObjects->Destroy(NewFactory);
		return nullptr;
	}
	return NewFactory;
}

#if WITH_EDITOR
FString UPCGUtilsExMeshOcclusionFilterProviderSettings::GetDisplayName() const
{
	return Config.Result == EPCGDynMeshSelfOcclusionResult::Occluded ? TEXT("Occluded") : TEXT("Unoccluded");
}
#endif

#undef LOCTEXT_NAMESPACE
