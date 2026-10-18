// Copyright Max Harris

#pragma once

#include "CoreMinimal.h"
#include "Elements/Painters/PCGDynMeshAxisGradientPainter.h"
#include "Factories/PCGUtilsDynMeshFactoryProvider.h"
#include "Factories/PCGUtilsDynMeshPainterFactory.h"
#include "GeometryScript/GeometryScriptTypes.h"

#include "PCGPainterExNoise.generated.h"

class UPCGExNoise3DFactoryData;

/** Immutable configuration for a Painter sampling a stack of PCGEx Noise3D definitions at every vertex. */
UCLASS(BlueprintType, ClassGroup=(Procedural), Category="PCGUtils|DynMesh|Painters")
class PCGUTILSPCGEXDYNMESHINTEROP_API UPCGPainterExNoiseFactoryData : public UPCGUtilsDynMeshPainterFactoryData
{
	GENERATED_BODY()

public:
	/** The noise stack, in PCGEx order (stable by Priority), exactly as Uber Noise combines it. */
	UPROPERTY()
	TArray<TObjectPtr<const UPCGExNoise3DFactoryData>> Noises;

	UPROPERTY()
	EPCGUtilsDynMeshPainterValueType ValueType = EPCGUtilsDynMeshPainterValueType::Scalar;

	UPROPERTY()
	FGeometryScriptColorFlags ColorChannels;

	UPROPERTY()
	EPCGUtilsDynMeshPainterCoordinateSpace Space = EPCGUtilsDynMeshPainterCoordinateSpace::World;

protected:
	virtual TSharedPtr<FPCGUtilsDynMeshPainterOperation> CreateOperationInternal() const override;
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
};

/**
 * Paints PCGEx noise: Uber Noise, but per DynMesh vertex instead of per point.
 *
 * Connect any number of PCGEx Noise3D definitions (Perlin, Simplex, Worley, FBM, ...) to the Noises pin. They are
 * stacked with their own weights and blend modes exactly as Uber Noise stacks them, and sampled at each vertex's
 * position. Scalar paints the noise's double value; Color paints its Vector4 value as RGBA on the chosen channels.
 * This replaces DynMesh To Points -> Uber Noise -> Painter by Vertex ID with one node.
 */
UCLASS(BlueprintType, ClassGroup=(Procedural), Category="PCGUtils|DynMesh|Painters",
	meta=(Keywords="DynMesh DynMeshEx PCGEx Painter noise uber perlin simplex worley fbm voronoi procedural texture"))
class PCGUTILSPCGEXDYNMESHINTEROP_API UPCGPainterExNoiseProviderSettings : public UPCGUtilsDynMeshFactoryProviderSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("PainterExNoise"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
#endif

	/** Scalar paints the noise value, which the consumer broadcasts to its Write Channels. Color paints the noise's RGBA. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Painter", meta=(PCG_Overridable))
	EPCGUtilsDynMeshPainterValueType ValueType = EPCGUtilsDynMeshPainterValueType::Scalar;

	/** The channels the color defines; the consumer leaves every other channel as it was. Color only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Painter",
		meta=(PCG_Overridable, EditCondition="ValueType == EPCGUtilsDynMeshPainterValueType::Color", EditConditionHides))
	FGeometryScriptColorFlags ColorChannels;

	/**
	 * Where the noise is sampled. World matches Uber Noise on points from DynMesh To Points, so moving a target
	 * moves it through the noise; DynMesh Local keeps the pattern stuck to the mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Painter", meta=(PCG_Overridable))
	EPCGUtilsDynMeshPainterCoordinateSpace Space = EPCGUtilsDynMeshPainterCoordinateSpace::World;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Painter", AdvancedDisplay, meta=(PCG_Overridable))
	int32 Priority = 0;

	virtual FName GetMainOutputPin() const override;
	virtual UPCGUtilsDynMeshFactoryData* CreateFactory(
		FPCGContext* InContext, UPCGUtilsDynMeshFactoryData* InFactory = nullptr) const override;

protected:
	virtual const FPCGDataTypeBaseId& GetFactoryTypeId() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
};
