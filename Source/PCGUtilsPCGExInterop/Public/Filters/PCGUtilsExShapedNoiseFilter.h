// Copyright Max Harris

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExFilterFactoryProvider.h"
#include "Core/PCGExPointFilter.h"
#include "Details/PCGExCompareShorthandsDetails.h"
#include "Noise/PCGUtilsExNoiseShaping.h"

#include "PCGUtilsExShapedNoiseFilter.generated.h"

namespace PCGExNoise3D
{
	class FNoiseGenerator;
}

USTRUCT(BlueprintType)
struct PCGUTILSPCGEXINTEROP_API FPCGUtilsExShapedNoiseFilterConfig
{
	GENERATED_BODY()

	/** Random offset, clamp, remap band and curve, scale: applied to the noise before it is compared. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGUtilsExNoiseShapingDetails Shaping;

	/** The shaped noise is operand A; this is how it is compared and what against (a constant or a per-point attribute). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCompareSelectorDouble Comparison = MakeDefaultComparison();

	/** Seeds the Random Offset together with each point's seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "Shaping.bRandomOffset", EditConditionHides))
	int32 Seed = 42;

	static FPCGExCompareSelectorDouble MakeDefaultComparison();
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Filter")
class PCGUTILSPCGEXINTEROP_API UPCGUtilsExShapedNoiseFilterFactory : public UPCGExPointFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGUtilsExShapedNoiseFilterConfig Config;

	TSharedPtr<PCGExNoise3D::FNoiseGenerator> NoiseGenerator;

	virtual bool Init(FPCGExContext* InContext) override;
	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
};

namespace PCGUtilsExShapedNoiseFilter
{
	class FFilter final : public PCGExPointFilter::ISimpleFilter
	{
	public:
		explicit FFilter(const TObjectPtr<const UPCGUtilsExShapedNoiseFilterFactory>& InFactory)
			: ISimpleFilter(InFactory), TypedFactory(InFactory)
		{
		}

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;

	private:
		TObjectPtr<const UPCGUtilsExShapedNoiseFilterFactory> TypedFactory;
		TSharedPtr<PCGExDetails::TSettingValue<double>> OperandB;
		/** Shaped noise per point, computed once in Init. */
		TArray<double> Shaped;
	};
}

/**
 * Filter : Shaped Noise. PCGEx's Filter : Noise with the shaping steps in front of the comparison: random offset,
 * clamp, remap band to 0-1 (+ curve), scale by attribute. Shapes noise exactly as PointEx | Assign Value From Noise.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Filter",
	meta = (Keywords = "PCGEx filter noise shaped remap clamp random offset curve threshold perlin simplex worley fbm"))
class PCGUTILSPCGEXINTEROP_API UPCGUtilsExShapedNoiseFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(ShapedNoiseFilterFactory, "Filter : Shaped Noise",
		"Compare shaped spatial noise (offset, clamp, remap, curve, scale) against a value.", PCGEX_FACTORY_NAME_PRIORITY)
	virtual FString GetDisplayName() const override;
#endif

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGUtilsExShapedNoiseFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
};
