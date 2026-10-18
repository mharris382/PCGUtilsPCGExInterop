// Copyright Max Harris

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGUtilsDynMeshDomainSelectionFactory.h"

#include "PCGSelectExByFilters.generated.h"

class UPCGExPointFilterFactoryData;

/** Selection factory that runs a stack of PCGEx point or data filters over a DynMesh's vertices. */
UCLASS(BlueprintType, ClassGroup=(Procedural), Category="PCGUtils|DynMesh|Selections")
class PCGUTILSPCGEXDYNMESHINTEROP_API UPCGSelectExByFiltersFactoryData : public UPCGUtilsDynMeshDomainSelectionFactoryData
{
	GENERATED_BODY()

public:
	/** The filter stack, ANDed in PCGEx order (stable by Priority), exactly as Uber Filter evaluates it. */
	UPROPERTY()
	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> Filters;

	UPROPERTY()
	bool bPositionsToWorld = true;

	UPROPERTY()
	bool bRotationFromNormal = true;

protected:
	virtual UE::Geometry::EGeometryElementType GetNativeElementTypeInternal() const override
	{
		return UE::Geometry::EGeometryElementType::Vertex;
	}
	virtual TSharedPtr<FPCGUtilsDynMeshSelectionOperation> CreateNativeOperationInternal() const override;
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
};

/**
 * Selects the vertices that pass a stack of PCGEx filters: DynMesh To Points -> Uber Filter -> Select From Vertex
 * IDs in one node.
 *
 * Each vertex is presented to the filters as one point:
 * - $Position is the vertex position, in world space by default (Positions To World), so spatial filters such as
 *   Inside Path compare against world-space paths.
 * - $Color is the vertex color (white when the mesh has none).
 * - $Rotation points its Z (Up) axis along the vertex normal (Rotation From Normal), so normal and dot filters
 *   work through $Rotation.Up.
 * - $Seed is derived from the position, as PCG does for new points. Every other point property keeps its default.
 * - The mesh's @Data domain is carried over, so a filter reads mesh attributes with an explicit @Data. selector,
 *   and data (collection) filters judge the mesh as a whole. Point-domain attributes do not exist.
 *
 * Filters are ANDed, as in Uber Filter. Turn on Invert Selection to select the vertices that fail. The result is a
 * vertex selection; edge and triangle consumers receive it through the standard domain conversion.
 */
UCLASS(BlueprintType, ClassGroup=(Procedural), Category="PCGUtils|DynMesh|Selections",
	meta=(Keywords="DynMesh DynMeshEx PCGEx Select filter filters uber point vertex data domain"))
class PCGUTILSPCGEXDYNMESHINTEROP_API UPCGSelectExByFiltersProviderSettings : public UPCGUtilsDynMeshDomainSelectionSourceSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("SelectExByFilters"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
#endif

	/**
	 * Present vertex positions in world space, placed by the mesh's PCG target actor (the module's actor-local
	 * convention). Disable only when the mesh is already in world space, or when the filters expect mesh-local
	 * positions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection", meta=(PCG_Overridable))
	bool bPositionsToWorld = true;

	/** Point each vertex's $Rotation Up (Z) axis along its normal. Disabled, every rotation is identity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection", meta=(PCG_Overridable))
	bool bRotationFromNormal = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection", AdvancedDisplay, meta=(PCG_Overridable))
	int32 Priority = 0;

	virtual UPCGUtilsDynMeshFactoryData* CreateFactory(
		FPCGContext* InContext, UPCGUtilsDynMeshFactoryData* InFactory = nullptr) const override;

protected:
	virtual const FPCGDataTypeBaseId& GetFactoryTypeId() const override;
	virtual TArray<FPCGPinProperties> SourceInputPinProperties() const override;
};
