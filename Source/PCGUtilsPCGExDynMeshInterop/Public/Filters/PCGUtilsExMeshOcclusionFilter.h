// Copyright Max Harris

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExFilterFactoryProvider.h"
#include "Core/PCGExPointFilter.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Elements/Selections/PCGSelectSelfOcclusion.h"

#include "PCGUtilsExMeshOcclusionFilter.generated.h"

class UPCGDynamicMeshData;

namespace PCGUtilsDynMeshOcclusion
{
	class FOccluderSet;
}

namespace PCGUtilsExMeshOcclusionFilterConstants
{
	inline const FName OccludersPin = TEXT("Occluders");
}

USTRUCT(BlueprintType)
struct PCGUTILSPCGEXDYNMESHINTEROP_API FPCGUtilsExMeshOcclusionFilterConfig
{
	GENERATED_BODY()

	/** Pass points whose ray is blocked by an occluder, or points whose ray is clear. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable))
	EPCGDynMeshSelfOcclusionResult Result = EPCGDynMeshSelfOcclusionResult::Occluded;

	/** Direction from each point toward the light or visibility source. A constant, or a vector attribute per point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable))
	FPCGExInputShorthandSelectorDirection Direction = FPCGExInputShorthandSelectorDirection(FString(TEXT("$Rotation.Up")), FVector::UpVector);

	/** Turn Direction by each point's rotation, so (0, 0, 1) is each point's own up axis. Off, Direction is in world space. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable))
	bool bDirectionInPointSpace = false;

	/** Maximum ray length in world units. 0 means unlimited. A constant, or a per-point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable))
	FPCGExInputShorthandSelectorDoubleAbs MaxDistance = FPCGExInputShorthandSelectorDoubleAbs(FString(TEXT("MaxDistance")), 0.0);

	/** Starts each ray this far along its direction, so a point lying on an occluder's surface does not hit it at distance zero. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable, ClampMin="0.0", Units="cm"))
	double OriginOffset = 0.1;

	/**
	 * The occluders use PCGUtils' actor-local DynMesh convention, so each is placed by its own target actor before
	 * tracing. Disable when the occluder meshes are already in world space.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, AdvancedDisplay, meta=(PCG_Overridable))
	bool bOccludersAreActorLocal = true;
};

/** Point filter: whether a ray from each point hits the DynMeshes on the Occluders pin. */
UCLASS(BlueprintType, ClassGroup=(Procedural), Category="PCGEx|Filter")
class PCGUTILSPCGEXDYNMESHINTEROP_API UPCGUtilsExMeshOcclusionFilterFactory : public UPCGExPointFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGUtilsExMeshOcclusionFilterConfig Config;

	/** Kept alive with the factory: the occluder set points into these meshes. */
	UPROPERTY()
	TArray<TObjectPtr<const UPCGDynamicMeshData>> OccluderMeshes;

	TSharedPtr<PCGUtilsDynMeshOcclusion::FOccluderSet> Occluders;

	virtual bool SupportsProxyEvaluation() const override;
	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual bool WantsPreparation(FPCGExContext* InContext) override { return true; }
	virtual PCGExFactories::EPreparationResult Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override;
};

namespace PCGUtilsExMeshOcclusionFilter
{
	class FFilter final : public PCGExPointFilter::ISimpleFilter
	{
	public:
		explicit FFilter(const TObjectPtr<const UPCGUtilsExMeshOcclusionFilterFactory>& InFactory)
			: ISimpleFilter(InFactory), TypedFactory(InFactory)
		{
		}

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const override;

		/** The test for one point: its transform, the raw Direction value and the Max Distance value. */
		bool TestAt(const FTransform& PointTransform, FVector RayDirection, double MaxDistance) const;

	private:
		TObjectPtr<const UPCGUtilsExMeshOcclusionFilterFactory> TypedFactory;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> DirectionGetter;
		TSharedPtr<PCGExDetails::TSettingValue<double>> MaxDistanceGetter;
		TConstPCGValueRange<FTransform> InTransforms;
	};
}

/**
 * Filter : Mesh Occlusion. Passes points whose ray (along Direction, up to Max Distance) is blocked by the DynMeshes
 * on the Occluders pin, or the points whose ray is clear. The point twin of PCGUtils' Select | By Mesh Occlusion,
 * sharing its ray tests.
 */
UCLASS(BlueprintType, ClassGroup=(Procedural), Category="PCGEx|Filter",
	meta=(Keywords="DynMesh PCGEx filter occlusion occluded occluder raycast ray trace shadow cover sheltered exposed mesh"))
class PCGUTILSPCGEXDYNMESHINTEROP_API UPCGUtilsExMeshOcclusionFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(MeshOcclusionFilterFactory, "Filter : Mesh Occlusion",
		"Passes points whose ray is blocked (or clear) by the DynMeshes on the Occluders pin.", PCGEX_FACTORY_NAME_PRIORITY)
	virtual FString GetDisplayName() const override;
	virtual bool ShowMissingDataPolicy_Internal() const override { return true; }
#endif

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGUtilsExMeshOcclusionFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	/** The factory holds spatial indexes built from this run's occluder meshes. */
	virtual bool IsCacheable() const override { return false; }
};
