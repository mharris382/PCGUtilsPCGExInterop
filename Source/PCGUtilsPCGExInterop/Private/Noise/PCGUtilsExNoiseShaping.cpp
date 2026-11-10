// Copyright Max Harris

#include "Noise/PCGUtilsExNoiseShaping.h"

#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Core/PCGExNoise3DOperation.h"
#include "Data/PCGBasePointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "PCGContext.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGUtilsExNoiseShaping"

FPCGUtilsExNoiseShapingDetails::FPCGUtilsExNoiseShapingDetails()
{
	RemapCurve.EditorCurveData.AddKey(0.f, 0.f);
	RemapCurve.EditorCurveData.AddKey(1.f, 1.f);
}

double FPCGUtilsExNoiseShapingDetails::Apply(double Noise, double RandomUnit, double Scale) const
{
	double Value = Noise;
	if (bRandomOffset)
	{
		Value += RandomUnit * FMath::Abs(RandomOffset);
	}
	if (bClamp)
	{
		Value = FMath::Clamp(Value, ClampMin, ClampMax);
	}
	if (bRemap)
	{
		const double Range = RemapInMax - RemapInMin;
		// A band with no width is a hard step at its edge.
		Value = FMath::IsNearlyZero(Range) ? (Value >= RemapInMax ? 1.0 : 0.0) : FMath::Clamp((Value - RemapInMin) / Range, 0.0, 1.0);
		if (bUseRemapCurve)
		{
			Value = RemapCurve.GetRichCurveConst()->Eval(static_cast<float>(Value));
		}
	}
	if (bScaleByAttribute)
	{
		Value *= Scale;
	}
	return Value;
}

namespace PCGUtilsExNoise
{
	double RandomUnit(int32 PointSeed, int32 Seed)
	{
		return FRandomStream(PCGHelpers::ComputeSeed(PointSeed, Seed)).FRandRange(-1.0, 1.0);
	}

	bool Validate(const FPCGUtilsExNoiseShapingDetails& Details, FPCGContext* Context, const FText& NodeName)
	{
		bool bValid = true;
		if (Details.bClamp && Details.ClampMin > Details.ClampMax)
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("ClampOrder", "{0}: Clamp Min ({1}) is greater than Clamp Max ({2}); Clamp Min must be at most Clamp Max."),
				NodeName, FText::AsNumber(Details.ClampMin), FText::AsNumber(Details.ClampMax)), Context);
			bValid = false;
		}
		if (Details.bScaleByAttribute && Details.ScaleAttributeName.IsNone())
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NoScaleName", "{0}: Scale By Attribute is on but Scale Attribute Name is None; name the attribute to scale by, or turn the option off."),
				NodeName), Context);
			bValid = false;
		}
		return bValid;
	}

	bool ShapeAll(const FPCGUtilsExNoiseShapingDetails& Details, const UPCGBasePointData* PointData, int32 Seed,
		TConstArrayView<double> Noise, TArray<double>& OutShaped, FPCGContext* Context, const FText& NodeName, int32 InputIndex)
	{
		const int32 Num = PointData->GetNumPoints();
		check(Noise.Num() == Num);

		TArray<double> Scales;
		if (Details.bScaleByAttribute)
		{
			FPCGAttributePropertyInputSelector Selector;
			Selector.Update(Details.ScaleAttributeName.ToString());
			Selector = Selector.CopyAndFixLast(PointData);
			TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(PointData, Selector, /*bQuiet=*/true);
			TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(PointData, Selector);
			if (!Accessor.IsValid() || !Keys.IsValid())
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("MissingScale", "{0}: input {1} has no attribute or property '{2}' (Scale Attribute Name). This input was skipped."),
					NodeName, InputIndex, FText::FromName(Details.ScaleAttributeName)), Context);
				return false;
			}
			if (!PCGAttributeAccessorHelpers::ExtractAllValues(Accessor.Get(), Keys.Get(), Scales))
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("BadScale", "{0}: input {1}, '{2}' (Scale Attribute Name) is of type {3}; expected a number or Boolean. This input was skipped."),
					NodeName, InputIndex, FText::FromName(Details.ScaleAttributeName),
					StaticEnum<EPCGMetadataTypes>()->GetDisplayNameTextByValue(Accessor->GetUnderlyingType())), Context);
				return false;
			}
			if (Scales.Num() != Num && Scales.Num() != 1)
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("ScaleCount", "{0}: input {1}, '{2}' (Scale Attribute Name) has {3} values for {4} points; expected one per point or a single value. This input was skipped."),
					NodeName, InputIndex, FText::FromName(Details.ScaleAttributeName), Scales.Num(), Num), Context);
				return false;
			}
		}

		TConstPCGValueRange<int32> Seeds = PointData->GetConstSeedValueRange();
		OutShaped.SetNumUninitialized(Num);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const double Random = Details.bRandomOffset ? RandomUnit(Seeds[Index], Seed) : 0.0;
			const double Scale = Scales.IsEmpty() ? 1.0 : Scales[Scales.Num() == 1 ? 0 : Index];
			OutShaped[Index] = Details.Apply(Noise[Index], Random, Scale);
		}
		return true;
	}

	bool FFactoryNoiseGenerator::InitFromFactories(TConstArrayView<TObjectPtr<const UPCGExNoise3DFactoryData>> Factories)
	{
		Operations.Reset();
		OperationsPtr.Reset();
		Weights.Reset();
		BlendModes.Reset();
		BlendFactors.Reset();
		TotalWeight = 0.0;

		for (const TObjectPtr<const UPCGExNoise3DFactoryData>& Factory : Factories)
		{
			// Noise3D factories ignore the context they create operations with.
			const TSharedPtr<FPCGExNoise3DOperation> Operation = Factory ? Factory->CreateOperation(nullptr) : nullptr;
			if (!Operation)
			{
				continue;
			}

			const double Weight = FMath::Max(SMALL_NUMBER, Factory->ConfigBase.WeightFactor);
			TotalWeight += Weight;

			Operations.Add(Operation);
			OperationsPtr.Add(Operation.Get());
			Weights.Add(Weight);
			BlendModes.Add(Operation->BlendMode);
			BlendFactors.Add(Weight / TotalWeight);
		}
		return Operations.Num() > 0;
	}

	void SortByPriority(TArray<TObjectPtr<const UPCGExNoise3DFactoryData>>& Factories)
	{
		Factories.StableSort([](const UPCGExNoise3DFactoryData& A, const UPCGExNoise3DFactoryData& B)
		{
			return A.Priority < B.Priority;
		});
	}
}

#undef LOCTEXT_NAMESPACE
