// Copyright Max Harris

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Helpers/PCGExNoiseGenerator.h"

#include "PCGUtilsExNoiseShaping.generated.h"

class UPCGBasePointData;
class UPCGExNoise3DFactoryData;
struct FPCGContext;

/**
 * The steps that turn raw PCGEx noise (0-1) into the value a node uses, applied in this order:
 * random offset -> clamp -> remap band to 0-1 (+ optional curve) -> scale by attribute.
 * Shared by PointEx | Assign Value From Noise and Filter : Shaped Noise, so both shape noise the same way.
 */
USTRUCT(BlueprintType)
struct PCGUTILSPCGEXINTEROP_API FPCGUtilsExNoiseShapingDetails
{
	GENERATED_BODY()

	FPCGUtilsExNoiseShapingDetails();

	/** Add pure random jitter to the noise before it is clamped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRandomOffset = false;

	/** Each point's noise moves by a random amount in [-Random Offset, +Random Offset], seeded by the point's seed and the node's Seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable, EditCondition = "bRandomOffset", ClampMin = "0"))
	double RandomOffset = 0.1;

	/** Hold the (offset) noise inside [Clamp Min, Clamp Max]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable))
	bool bClamp = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable, EditCondition = "bClamp", EditConditionHides))
	double ClampMin = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable, EditCondition = "bClamp", EditConditionHides))
	double ClampMax = 1.0;

	/**
	 * Stretch one band of the noise to 0-1: Remap In Min becomes 0, Remap In Max becomes 1, values outside the band
	 * are held at 0 or 1. A narrow band sharpens the noise; In Min above In Max inverts it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable))
	bool bRemap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable, EditCondition = "bRemap", EditConditionHides))
	double RemapInMin = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable, EditCondition = "bRemap", EditConditionHides))
	double RemapInMax = 1.0;

	/** Shape the remapped 0-1 value with a curve (X: remapped value, Y: result). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable, EditCondition = "bRemap", EditConditionHides))
	bool bUseRemapCurve = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_NotOverridable, EditCondition = "bRemap && bUseRemapCurve", EditConditionHides))
	FRuntimeFloatCurve RemapCurve;

	/** Multiply the shaped noise by a per-point attribute, as the last step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable))
	bool bScaleByAttribute = false;

	/**
	 * Required when Scale By Attribute is on. Numeric (or Boolean) attribute read per point, e.g. a falloff mask.
	 * '$Density' and other point properties work too.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Shaping", meta = (PCG_Overridable, EditCondition = "bScaleByAttribute", EditConditionHides))
	FName ScaleAttributeName = TEXT("NoiseScale");

	/** The whole chain for one value. RandomUnit is in [-1, 1] (ignored without Random Offset); Scale is ignored without Scale By Attribute. */
	double Apply(double Noise, double RandomUnit, double Scale) const;
};

namespace PCGUtilsExNoise
{
	/** Deterministic random value in [-1, 1] from a point's seed and the node's seed. */
	PCGUTILSPCGEXINTEROP_API double RandomUnit(int32 PointSeed, int32 Seed);

	/**
	 * Checks the shaping settings on their own; every failed condition is logged separately, prefixed with
	 * NodeName. Returns false if any failed.
	 */
	PCGUTILSPCGEXINTEROP_API bool Validate(const FPCGUtilsExNoiseShapingDetails& Details, FPCGContext* Context, const FText& NodeName);

	/**
	 * Shapes already-sampled noise for every point of PointData: Noise in, shaped values out (same count).
	 * Reads the scale attribute when needed; logs and returns false if it cannot be read.
	 */
	PCGUTILSPCGEXINTEROP_API bool ShapeAll(const FPCGUtilsExNoiseShapingDetails& Details, const UPCGBasePointData* PointData, int32 Seed,
		TConstArrayView<double> Noise, TArray<double>& OutShaped, FPCGContext* Context, const FText& NodeName, int32 InputIndex);

	/**
	 * A PCGEx noise generator built from stored Noise3D factories instead of a PCGEx context's pins, so plain PCG
	 * elements can sample noise. Mirrors FNoiseGenerator::Init: same order, weights and blend factors. Pass the
	 * factories already sorted by Priority (SortByPriority).
	 */
	class PCGUTILSPCGEXINTEROP_API FFactoryNoiseGenerator final : public PCGExNoise3D::FNoiseGenerator
	{
	public:
		bool InitFromFactories(TConstArrayView<TObjectPtr<const UPCGExNoise3DFactoryData>> Factories);
	};

	/** Uber Noise's stacking order: stable by Priority, so equal priorities keep their pin order. */
	PCGUTILSPCGEXINTEROP_API void SortByPriority(TArray<TObjectPtr<const UPCGExNoise3DFactoryData>>& Factories);
}
