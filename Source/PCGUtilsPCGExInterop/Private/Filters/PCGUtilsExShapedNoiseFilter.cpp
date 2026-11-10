// Copyright Max Harris

#include "Filters/PCGUtilsExShapedNoiseFilter.h"

#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExContext.h"
#include "Core/PCGExNoise3DCommon.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExNoiseGenerator.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGUtilsExShapedNoiseFilter"

namespace PCGUtilsExShapedNoiseFilter
{
	static const FText NodeName = LOCTEXT("NodeName", "Filter : Shaped Noise");
}

FPCGExCompareSelectorDouble FPCGUtilsExShapedNoiseFilterConfig::MakeDefaultComparison()
{
	FPCGExCompareSelectorDouble Comparison(TEXT("Threshold"), 0.5);
	Comparison.Comparison = EPCGExComparison::EqualOrGreater;
	return Comparison;
}

bool UPCGUtilsExShapedNoiseFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext))
	{
		return false;
	}
	if (!PCGUtilsExNoise::Validate(Config.Shaping, InContext, PCGUtilsExShapedNoiseFilter::NodeName))
	{
		return false;
	}
	NoiseGenerator = MakeShared<PCGExNoise3D::FNoiseGenerator>();
	return NoiseGenerator->Init(InContext);
}

void UPCGUtilsExShapedNoiseFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	Config.Comparison.RegisterBufferDependencies(InContext, FacadePreloader);
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGUtilsExShapedNoiseFilterFactory::CreateFilter() const
{
	return MakeShared<PCGUtilsExShapedNoiseFilter::FFilter>(this);
}

bool PCGUtilsExShapedNoiseFilter::FFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade))
	{
		return false;
	}

	OperandB = TypedFactory->Config.Comparison.GetValueSetting(PCGEX_QUIET_HANDLING);
	OperandB->bRegisterConsumable &= TypedFactory->bCleanupConsumableAttributes;
	if (!OperandB->Init(PointDataFacade))
	{
		return false;
	}

	// Sample and shape every point once, so Test is a lookup and the scale attribute is read in one pass.
	const UPCGBasePointData* Points = PointDataFacade->GetIn();
	const int32 Num = Points->GetNumPoints();
	TArray<FVector> Positions;
	Positions.SetNumUninitialized(Num);
	const TConstPCGValueRange<FTransform> Transforms = Points->GetConstTransformValueRange();
	for (int32 Index = 0; Index < Num; ++Index)
	{
		Positions[Index] = Transforms[Index].GetLocation();
	}
	TArray<double> Noise;
	Noise.SetNumUninitialized(Num);
	TypedFactory->NoiseGenerator->GenerateParallel(Positions, Noise);
	return PCGUtilsExNoise::ShapeAll(TypedFactory->Config.Shaping, Points, TypedFactory->Config.Seed, Noise, Shaped, InContext, NodeName,
		PointDataFacade->Source->IOIndex);
}

bool PCGUtilsExShapedNoiseFilter::FFilter::Test(const int32 PointIndex) const
{
	return TypedFactory->Config.Comparison.Compare(Shaped[PointIndex], OperandB->Read(PointIndex));
}

TArray<FPCGPinProperties> UPCGUtilsExShapedNoiseFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins = Super::InputPinProperties();
	FPCGPinProperties& Pin = Pins.Emplace_GetRef(PCGExNoise3D::Labels::SourceNoise3DLabel, FPCGExDataTypeInfoNoise3D::AsId(), true, true);
	Pin.SetRequiredPin();
#if WITH_EDITOR
	Pin.Tooltip = LOCTEXT("NoisesTip", "Required. PCGEx Noise3D definitions, stacked as Uber Noise stacks them.");
#endif
	return Pins;
}

UPCGExFactoryData* UPCGUtilsExShapedNoiseFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGUtilsExShapedNoiseFilterFactory* NewFactory = InContext->ManagedObjects->New<UPCGUtilsExShapedNoiseFilterFactory>();
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
FString UPCGUtilsExShapedNoiseFilterProviderSettings::GetDisplayName() const
{
	const UEnum* Enum = StaticEnum<EPCGExComparison>();
	const FString Operator = Enum->GetDisplayNameTextByValue(static_cast<int64>(Config.Comparison.Comparison)).ToString().TrimStartAndEnd();
	const FString Operand = Config.Comparison.Input == EPCGExInputValueType::Constant
		? FString::SanitizeFloat(Config.Comparison.Constant) : Config.Comparison.Attribute.ToString();
	return FString::Printf(TEXT("Noise %s %s"), *Operator, *Operand);
}
#endif

#undef LOCTEXT_NAMESPACE
