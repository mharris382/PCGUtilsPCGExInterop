// Copyright Max Harris

#include "Elements/PCGUtilsExAssignValueFromNoise.h"

#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataDomain.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGUtilsSettingsCategories.h"
#include "Utils/PCGLogErrors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGUtilsExAssignValueFromNoise)

#define LOCTEXT_NAMESPACE "PCGUtilsExAssignValueFromNoise"

namespace PCGUtilsExAssignValueFromNoise
{
	static const FText NodeName = LOCTEXT("NodeName", "Assign Value From Noise");

	static FText TypeText(int16 Type)
	{
		return StaticEnum<EPCGMetadataTypes>()->GetDisplayNameTextByValue(Type);
	}

	/** Min or Max for every point: one value for a constant, or the attribute's values (one per point, or a single one). */
	template <typename T, typename ShorthandType>
	static bool ReadOperand(const ShorthandType& Shorthand, const TCHAR* Label, const UPCGBasePointData* Data, int32 InputIndex,
		FPCGContext* Context, TArray<T>& OutValues)
	{
		if (Shorthand.Input == EPCGExInputValueType::Constant)
		{
			OutValues = {Shorthand.Constant};
			return true;
		}

		if (Shorthand.Attribute.IsNone())
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("OperandNoName", "{0}: {1} reads an attribute but its attribute name is None; name the attribute or switch {1} to Constant."),
				NodeName, FText::FromString(Label)), Context);
			return false;
		}

		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(Shorthand.Attribute.ToString());
		Selector = Selector.CopyAndFixLast(Data);
		TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Data, Selector, /*bQuiet=*/true);
		TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(Data, Selector);
		if (!Accessor.IsValid() || !Keys.IsValid())
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("OperandMissing", "{0}: input {1} has no attribute or property '{2}' ({3}). This input was skipped."),
				NodeName, InputIndex, FText::FromName(Shorthand.Attribute), FText::FromString(Label)), Context);
			return false;
		}
		if (!PCGAttributeAccessorHelpers::ExtractAllValues(Accessor.Get(), Keys.Get(), OutValues))
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("OperandType", "{0}: input {1}, '{2}' ({3}) is of type {4}, which does not convert to {5} (Value Type). This input was skipped."),
				NodeName, InputIndex, FText::FromName(Shorthand.Attribute), FText::FromString(Label), TypeText(Accessor->GetUnderlyingType()),
				TypeText(PCG::Private::MetadataTypes<T>::Id)), Context);
			return false;
		}
		const int32 Num = Data->GetNumPoints();
		if (OutValues.Num() != Num && OutValues.Num() != 1)
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("OperandCount", "{0}: input {1}, '{2}' ({3}) has {4} values for {5} points; expected one per point or a single value. This input was skipped."),
				NodeName, InputIndex, FText::FromName(Shorthand.Attribute), FText::FromString(Label), OutValues.Num(), Num), Context);
			return false;
		}
		return true;
	}

	template <typename T>
	static T LerpValue(const T& A, const T& B, double Alpha)
	{
		return FMath::Lerp(A, B, Alpha);
	}

	template <>
	FRotator LerpValue(const FRotator& A, const FRotator& B, double Alpha)
	{
		// Per component on purpose: Min 0 / Max 360 yaw must turn, which a slerp between equal rotations would not.
		return A + (B - A) * Alpha;
	}

	/** Writes one value per point. A missing attribute is created as T; existing ones keep their type when T converts to it. */
	template <typename T>
	static bool WriteValues(UPCGBasePointData* Output, FName Name, const TCHAR* SettingLabel, TArrayView<const T> Values, int32 InputIndex, FPCGContext* Context)
	{
		FPCGAttributePropertyOutputSelector Selector;
		Selector.Update(Name.ToString());
		if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			FPCGMetadataDomain* Domain = Output->MutableMetadata() ? Output->MutableMetadata()->GetMetadataDomainFromSelector(Selector) : nullptr;
			if (!Domain)
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("BadDomain", "{0}: input {1} has no metadata domain for '{2}' ({3}). This input was skipped."),
					NodeName, InputIndex, FText::FromName(Name), FText::FromString(SettingLabel)), Context);
				return false;
			}
			if (!Domain->GetConstAttribute(Selector.GetAttributeName()))
			{
				Domain->CreateAttribute<T>(Selector.GetAttributeName(), T{}, /*bAllowsInterpolation=*/true, /*bOverrideParent=*/false);
			}
		}

		TUniquePtr<IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateAccessor(Output, Selector, /*bQuiet=*/true);
		TUniquePtr<IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateKeys(Output, Selector);
		if (!Accessor.IsValid() || !Keys.IsValid())
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NoOutput", "{0}: input {1} cannot write '{2}' ({3}); no such attribute or property can be created on point data. This input was skipped."),
				NodeName, InputIndex, FText::FromName(Name), FText::FromString(SettingLabel)), Context);
			return false;
		}
		if (Keys->GetNum() != Values.Num())
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("OutputCount", "{0}: input {1}, '{2}' ({3}) holds {4} values but there are {5} points; write to a point attribute or property, not @Data. This input was skipped."),
				NodeName, InputIndex, FText::FromName(Name), FText::FromString(SettingLabel), Keys->GetNum(), Values.Num()), Context);
			return false;
		}
		if (!Accessor->SetRange<T>(Values, 0, *Keys, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("OutputType", "{0}: input {1}, '{2}' ({3}) is of type {4}, which a {5} value (Value Type) cannot be written to. This input was skipped."),
				NodeName, InputIndex, FText::FromName(Name), FText::FromString(SettingLabel), TypeText(Accessor->GetUnderlyingType()),
				TypeText(PCG::Private::MetadataTypes<T>::Id)), Context);
			return false;
		}
		return true;
	}

	template <typename T, typename ShorthandType>
	static bool LerpAndWrite(const UPCGUtilsExAssignValueFromNoiseSettings& Settings, const ShorthandType& Min, const ShorthandType& Max,
		const UPCGBasePointData* Input, UPCGBasePointData* Output, TConstArrayView<double> Alphas, int32 InputIndex, FPCGContext* Context)
	{
		TArray<T> MinValues, MaxValues;
		if (!ReadOperand<T>(Min, TEXT("Min"), Input, InputIndex, Context, MinValues)
			|| !ReadOperand<T>(Max, TEXT("Max"), Input, InputIndex, Context, MaxValues))
		{
			return false;
		}

		TArray<T> Results;
		Results.SetNumUninitialized(Alphas.Num());
		for (int32 Index = 0; Index < Alphas.Num(); ++Index)
		{
			const T& A = MinValues[MinValues.Num() == 1 ? 0 : Index];
			const T& B = MaxValues[MaxValues.Num() == 1 ? 0 : Index];
			Results[Index] = LerpValue(A, B, Alphas[Index]);
		}
		return WriteValues<T>(Output, Settings.OutputAttributeName, TEXT("Output Attribute Name"), Results, InputIndex, Context);
	}
}

#if WITH_EDITOR
FText UPCGUtilsExAssignValueFromNoiseSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "PointEx | Assign Value From Noise");
}

FText UPCGUtilsExAssignValueFromNoiseSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Uber Noise -> random offset -> clamp -> remap band to 0-1 (+ curve) -> scale by attribute -> Lerp(Min, Max, noise), in one node.\n"
		"Samples the PCGEx Noise3D stack on the Noises pin at each point and writes the lerp result to Output Attribute Name ($Density by default).");
}

FString UPCGUtilsExAssignValueFromNoiseSettings::GetAdditionalTitleInformation() const
{
	return OutputAttributeName.ToString();
}

EPCGSettingsType UPCGUtilsExAssignValueFromNoiseSettings::GetType() const
{
	return PCGUtilsSettingsCategories::AsSettingsType(PCGUtilsSettingsCategories::EValue::Points);
}
#endif

TArray<FPCGPinProperties> UPCGUtilsExAssignValueFromNoiseSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	FPCGPinProperties& In = Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	In.SetRequiredPin();
	FPCGPinProperties& Noises = Pins.Emplace_GetRef(PCGUtilsExAssignValueFromNoiseConstants::NoisesPin, FPCGExDataTypeInfoNoise3D::AsId(), true, true);
	Noises.SetRequiredPin();
#if WITH_EDITOR
	Noises.Tooltip = LOCTEXT("NoisesTip", "Required. PCGEx Noise3D definitions, stacked by Priority with their own weights and blend modes, as Uber Noise stacks them.");
#endif
	return Pins;
}

TArray<FPCGPinProperties> UPCGUtilsExAssignValueFromNoiseSettings::OutputPinProperties() const
{
	return {FPCGPinProperties(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point)};
}

FPCGElementPtr UPCGUtilsExAssignValueFromNoiseSettings::CreateElement() const
{
	return MakeShared<FPCGUtilsExAssignValueFromNoiseElement>();
}

bool FPCGUtilsExAssignValueFromNoiseElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGUtilsExAssignValueFromNoiseElement::ExecuteInternal);
	using namespace PCGUtilsExAssignValueFromNoise;

	const UPCGUtilsExAssignValueFromNoiseSettings* Settings = Context->GetInputSettings<UPCGUtilsExAssignValueFromNoiseSettings>();
	check(Settings);

	bool bValid = PCGUtilsExNoise::Validate(Settings->Shaping, Context, NodeName);
	if (Settings->OutputAttributeName.IsNone())
	{
		PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NoOutputName", "{0}: Output Attribute Name is None; it must name the attribute or property to write."), NodeName), Context);
		bValid = false;
	}
	if (Settings->bWriteNoise && Settings->NoiseAttributeName.IsNone())
	{
		PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NoNoiseName", "{0}: Write Noise is on but Noise Attribute Name is None; name the attribute, or turn Write Noise off."), NodeName), Context);
		bValid = false;
	}

	TArray<TObjectPtr<const UPCGExNoise3DFactoryData>> Factories;
	for (const FPCGTaggedData& NoiseInput : Context->InputData.GetInputsByPin(PCGUtilsExAssignValueFromNoiseConstants::NoisesPin))
	{
		if (const UPCGExNoise3DFactoryData* Factory = Cast<const UPCGExNoise3DFactoryData>(NoiseInput.Data))
		{
			Factories.Add(Factory);
		}
	}
	PCGUtilsExNoise::SortByPriority(Factories);
	PCGUtilsExNoise::FFactoryNoiseGenerator Generator;
	if (!Generator.InitFromFactories(Factories))
	{
		PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NoNoise", "{0}: the Noises pin has {1} Noise3D definitions that produce a noise; connect at least one."),
			NodeName, Generator.Num()), Context);
		bValid = false;
	}
	if (!bValid)
	{
		return true;
	}

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		const FPCGTaggedData& Input = Inputs[InputIndex];
		const UPCGBasePointData* InPoints = Cast<const UPCGBasePointData>(Input.Data);
		if (!InPoints)
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NotPoints", "{0}: input {1} is not point data; noise is sampled at point positions. This input was skipped."), NodeName, InputIndex), Context);
			continue;
		}

		const int32 Num = InPoints->GetNumPoints();
		TArray<FVector> Positions;
		Positions.SetNumUninitialized(Num);
		const TConstPCGValueRange<FTransform> Transforms = InPoints->GetConstTransformValueRange();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			Positions[Index] = Transforms[Index].GetLocation();
		}
		TArray<double> Noise;
		Noise.SetNumUninitialized(Num);
		Generator.GenerateParallel(Positions, Noise);

		TArray<double> Alphas;
		if (!PCGUtilsExNoise::ShapeAll(Settings->Shaping, InPoints, Settings->Seed, Noise, Alphas, Context, NodeName, InputIndex))
		{
			continue;
		}

		// The output is the input with values written: duplicating keeps every attribute, tag and property.
		UPCGBasePointData* OutPoints = CastChecked<UPCGBasePointData>(InPoints->DuplicateData(Context));

		bool bWritten = false;
		switch (Settings->ValueType)
		{
		case EPCGUtilsExNoiseLerpType::Scalar: bWritten = LerpAndWrite<double>(*Settings, Settings->MinScalar, Settings->MaxScalar, InPoints, OutPoints, Alphas, InputIndex, Context); break;
		case EPCGUtilsExNoiseLerpType::Vector2: bWritten = LerpAndWrite<FVector2D>(*Settings, Settings->MinVector2, Settings->MaxVector2, InPoints, OutPoints, Alphas, InputIndex, Context); break;
		case EPCGUtilsExNoiseLerpType::Vector: bWritten = LerpAndWrite<FVector>(*Settings, Settings->MinVector, Settings->MaxVector, InPoints, OutPoints, Alphas, InputIndex, Context); break;
		case EPCGUtilsExNoiseLerpType::Vector4: bWritten = LerpAndWrite<FVector4>(*Settings, Settings->MinVector4, Settings->MaxVector4, InPoints, OutPoints, Alphas, InputIndex, Context); break;
		case EPCGUtilsExNoiseLerpType::Rotator: bWritten = LerpAndWrite<FRotator>(*Settings, Settings->MinRotator, Settings->MaxRotator, InPoints, OutPoints, Alphas, InputIndex, Context); break;
		}
		if (!bWritten)
		{
			continue;
		}
		if (Settings->bWriteNoise && !WriteValues<double>(OutPoints, Settings->NoiseAttributeName, TEXT("Noise Attribute Name"), Alphas, InputIndex, Context))
		{
			continue;
		}

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
		Output.Data = OutPoints;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
