// Copyright Max Harris

#pragma once

#include "CoreMinimal.h"
#include "Details/PCGExMatchingDetails.h"
#include "Elements/PCGDynamicMeshBaseElement.h"
#include "PCGUtilsSettingsCategories.h"

#include "PCGDynMeshExMerge.generated.h"

/**
 * Merges DynMeshes into groups chosen by PCGEx match rules, one DynMesh out per group. It is to DynMeshes what
 * the PCGEx Merge Points node is to points.
 *
 * Every mesh is tested against every other with the Match Rules. Meshes that match are appended into one mesh;
 * a mesh that matches no other passes through unchanged, the same object it came in as. With matching Disabled
 * everything on the pin is merged into one.
 *
 * A typical rule is Match | Attributes comparing an @Data attribute with itself (@Data.CutID == @Data.CutID),
 * which leaves exactly one mesh per value of that attribute, however many arrived carrying it.
 *
 * A merged mesh keeps the @Data attributes and target actor of the first mesh in its group, the tags of all of
 * them, and all of their materials, with each mesh's material slots moved onto one shared list.
 *
 * The pieces are appended, not unioned: where they overlap, the result still has faces inside it. Follow with
 * DynMesh | Self Union when a clean solid is needed.
 *
 * Whole meshes only. Merging is defined on datasets, so selection data and the Selector pin have no meaning here,
 * which is why this node sits on the engine DynMesh base and not the PCGUtils process base.
 */
UCLASS(BlueprintType, ClassGroup=(Procedural), Category="PCGUtils|DynMesh|Topology",
	meta=(Keywords="DynMesh mesh merge append combine group partition PCGEx match matching rules by attribute tag"))
class PCGUTILSPCGEXDYNMESHINTEROP_API UPCGDynMeshExMergeSettings : public UPCGDynamicMeshBaseSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EPCGSettingsType GetType() const override
	{
		return PCGUtilsSettingsCategories::AsSettingsType(PCGUtilsSettingsCategories::EValue::DynMeshTopology);
	}
	virtual FName GetDefaultNodeName() const override { return TEXT("DynMeshExMerge"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.413f, 0.25f, 1.0f, 1.0f); }
#endif

	/**
	 * Which meshes are merged together. Disabled merges everything on the pin into one mesh.
	 *
	 * Declared as a filter usage: matching only decides the groups, so the unmatched-output and match-limit
	 * options, which exist for nodes that pair two different inputs, are not offered. A mesh that matches no
	 * other is a group of one and passes through.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Matching", meta=(PCG_NotOverridable))
	FPCGExMatchingDetails DataMatching =
		FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Filter, EPCGExMapMatchMode::Disabled);

	/** Each mesh joins one group only, the first it matches. Off: a mesh is merged into every group it matches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Matching", AdvancedDisplay, meta=(PCG_Overridable))
	bool bExclusiveGroups = true;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class PCGUTILSPCGEXDYNMESHINTEROP_API FPCGDynMeshExMergeElement : public IPCGDynamicMeshBaseElement
{
public:
	/**
	 * A PCGEx match rule is point-derived data. PCG turns every point-derived input into plain point data before a
	 * node runs unless the node says it can take them as they are, and a rule turned into plain points is no longer
	 * a rule: the node would report that it received none.
	 */
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
