// Copyright Max Harris

#pragma once

#include "CoreMinimal.h"
#include "Details/PCGExMatchingDetails.h"
#include "Elements/Creation/PCGDynMeshRealizeBuilders.h"

#include "PCGDynMeshExRealizeBuilders.generated.h"

/**
 * Realize Builders, with the Builder-to-seed pairing under the graph's control.
 *
 * The base node applies every Builder on the pin to every seed point, which is what makes several Builders
 * compose one compound shape. That is the wrong contract when the seeds are heterogeneous - a scatter of
 * points where some should become walls, some doors and some windows - because expressing it needs one
 * Realize Builders node per kind and a Point Filter in front of each.
 *
 * This node keeps every behaviour of the base and adds a Match Rules pin. Each seed point is tested against
 * each Builder's data using PCGEx's match rules, and only the Builders that match are evaluated for that
 * seed. Seeds that match nothing produce no output, as do Builders that no seed matched, so Output Mode
 * keeps meaning what it did.
 *
 * The seed points are the matching *source* and the Builders are the *candidates*, which is the orientation
 * the rules read in: Tag To Attribute compares a tag on the Builder against an attribute on the seed.
 */
UCLASS(BlueprintType, ClassGroup=(Procedural), Category="PCGUtils|DynMesh",
	meta=(Keywords="DynMesh Builder Builders Build realize materialize seeds compound PCGEx match matching rules pair assign"))
class PCGUTILSPCGEXDYNMESHINTEROP_API UPCGDynMeshExRealizeBuildersSettings : public UPCGDynMeshRealizeBuildersSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("DynMeshExRealizeBuilders"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
#endif

	/**
	 * How a seed point is paired with a Builder.
	 *
	 * Declared as a filter usage: matching decides which Builders apply to a seed and nothing else, so the
	 * unmatched-output and match-limit options - which exist for nodes that forward whole data sets - are
	 * not offered. A seed that matches no Builder simply produces no geometry, and is reported on the graph.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Matching", meta=(PCG_NotOverridable))
	FPCGExMatchingDetails DataMatching =
		FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Filter, EPCGExMapMatchMode::All);

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class PCGUTILSPCGEXDYNMESHINTEROP_API FPCGDynMeshExRealizeBuildersElement : public FPCGDynMeshRealizeBuildersElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
