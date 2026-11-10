// Copyright Max Harris

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Data/PCGPointArrayData.h"
#include "Elements/PCGUtilsExAssignValueFromNoise.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Noise/PCGUtilsExNoiseShaping.h"
#include "PCGContext.h"
#include "UObject/Package.h"

namespace PCGUtilsExAssignValueFromNoiseTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	class FExecute : public FPCGUtilsExAssignValueFromNoiseElement
	{
	public:
		using FPCGUtilsExAssignValueFromNoiseElement::ExecuteInternal;
	};

	/** The concrete class is not exported, so make it by name. */
	const UPCGExNoise3DFactoryData* MakePerlin()
	{
		UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/PCGExNoise3D.PCGExNoise3DFactoryPerlin"));
		return Class ? NewObject<UPCGExNoise3DFactoryData>(GetTransientPackage(), Class) : nullptr;
	}

	/** 16 points on a grid 53 units apart, distinct seeds, a Tint attribute 0..15 and an untouched Keep attribute. */
	UPCGPointArrayData* MakePoints()
	{
		UPCGPointArrayData* Points = NewObject<UPCGPointArrayData>();
		Points->SetNumPoints(16);
		FPCGMetadataDomain* Elements = Points->Metadata->GetMetadataDomain(PCGMetadataDomainID::Elements);
		FPCGMetadataAttribute<double>* Tint = Elements->CreateAttribute<double>(TEXT("Tint"), 0.0, true, false);
		FPCGMetadataAttribute<int32>* Keep = Elements->CreateAttribute<int32>(TEXT("Keep"), 0, false, false);
		auto Transforms = Points->GetTransformValueRange();
		auto Seeds = Points->GetSeedValueRange();
		auto Keys = Points->GetMetadataEntryValueRange();
		for (int32 Index = 0; Index < 16; ++Index)
		{
			Transforms[Index] = FTransform(FVector((Index % 4) * 53.0, (Index / 4) * 53.0, 7.0));
			Seeds[Index] = 1000 + Index * 17;
			Keys[Index] = Elements->AddEntry();
			Tint->SetValue(Keys[Index], Index);
			Keep->SetValue(Keys[Index], 500 + Index);
		}
		return Points;
	}

	void Run(UPCGUtilsExAssignValueFromNoiseSettings* Settings, UPCGData* Points, const UPCGExNoise3DFactoryData* Noise, FPCGContext& Context)
	{
		Context.InputData.TaggedData.Emplace_GetRef().Data = Settings;
		FPCGTaggedData& In = Context.InputData.TaggedData.Emplace_GetRef();
		In.Data = Points; In.Pin = PCGPinConstants::DefaultInputLabel; In.Tags.Add(TEXT("KeepTag"));
		if (Noise)
		{
			FPCGTaggedData& NoiseIn = Context.InputData.TaggedData.Emplace_GetRef();
			NoiseIn.Data = Noise; NoiseIn.Pin = PCGUtilsExAssignValueFromNoiseConstants::NoisesPin;
		}
		FExecute().ExecuteInternal(&Context);
	}

	/** Shaped noise for each point of Points, computed independently of the node. */
	TArray<double> ExpectedAlphas(const UPCGPointArrayData* Points, const UPCGExNoise3DFactoryData* Noise, const FPCGUtilsExNoiseShapingDetails& Shaping, int32 Seed)
	{
		PCGUtilsExNoise::FFactoryNoiseGenerator Generator;
		TArray<TObjectPtr<const UPCGExNoise3DFactoryData>> Factories = {Noise};
		Generator.InitFromFactories(Factories);
		TArray<double> Result;
		for (int32 Index = 0; Index < Points->GetNumPoints(); ++Index)
		{
			const double Raw = Generator.GetDouble(Points->GetConstTransformValueRange()[Index].GetLocation());
			Result.Add(Shaping.Apply(Raw, PCGUtilsExNoise::RandomUnit(Points->GetConstSeedValueRange()[Index], Seed), 1.0));
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGUtilsExNoiseShapingApplyTest, "PCGUtils.PCGExInterop.NoiseShaping.Apply", PCGUtilsExAssignValueFromNoiseTests::Flags)
bool FPCGUtilsExNoiseShapingApplyTest::RunTest(const FString&)
{
	FPCGUtilsExNoiseShapingDetails Shaping; // clamp 0-1, remap 0-1: identity inside 0-1
	TestEqual(TEXT("Defaults pass 0.3 through"), Shaping.Apply(0.3, 0.0, 1.0), 0.3, 1e-9);
	TestEqual(TEXT("Defaults clamp 1.4"), Shaping.Apply(1.4, 0.0, 1.0), 1.0, 1e-9);

	Shaping.bRandomOffset = true; Shaping.RandomOffset = 0.2;
	TestEqual(TEXT("Offset adds RandomUnit * amount"), Shaping.Apply(0.5, -0.5, 1.0), 0.4, 1e-9);
	TestEqual(TEXT("Offset then clamp"), Shaping.Apply(0.95, 1.0, 1.0), 1.0, 1e-9);
	Shaping.bRandomOffset = false;

	Shaping.RemapInMin = 0.4; Shaping.RemapInMax = 0.6;
	TestEqual(TEXT("Band middle"), Shaping.Apply(0.5, 0.0, 1.0), 0.5, 1e-9);
	TestEqual(TEXT("Below band"), Shaping.Apply(0.2, 0.0, 1.0), 0.0, 1e-9);
	TestEqual(TEXT("Above band"), Shaping.Apply(0.9, 0.0, 1.0), 1.0, 1e-9);
	Shaping.RemapInMin = 0.6; Shaping.RemapInMax = 0.4;
	TestEqual(TEXT("Inverted band"), Shaping.Apply(0.45, 0.0, 1.0), 0.75, 1e-9);
	Shaping.RemapInMin = 0.5; Shaping.RemapInMax = 0.5;
	TestEqual(TEXT("Zero-width band steps below"), Shaping.Apply(0.49, 0.0, 1.0), 0.0, 1e-9);
	TestEqual(TEXT("Zero-width band steps at"), Shaping.Apply(0.5, 0.0, 1.0), 1.0, 1e-9);

	Shaping.RemapInMin = 0.0; Shaping.RemapInMax = 1.0;
	Shaping.bUseRemapCurve = true;
	Shaping.RemapCurve.EditorCurveData.Reset();
	Shaping.RemapCurve.EditorCurveData.AddKey(0.f, 1.f);
	Shaping.RemapCurve.EditorCurveData.AddKey(1.f, 0.f);
	TestEqual(TEXT("Curve inverts"), Shaping.Apply(0.25, 0.0, 1.0), 0.75, 1e-6);
	Shaping.bUseRemapCurve = false;

	Shaping.bScaleByAttribute = true;
	TestEqual(TEXT("Scale applies last"), Shaping.Apply(0.5, 0.0, 0.5), 0.25, 1e-9);
	Shaping.bClamp = false; Shaping.bRemap = false; Shaping.bScaleByAttribute = false;
	TestEqual(TEXT("All off is raw noise"), Shaping.Apply(1.7, 0.0, 1.0), 1.7, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGUtilsExAssignValueFromNoiseScalarTest, "PCGUtils.PCGExInterop.AssignValueFromNoise.ScalarToDensity", PCGUtilsExAssignValueFromNoiseTests::Flags)
bool FPCGUtilsExAssignValueFromNoiseScalarTest::RunTest(const FString&)
{
	using namespace PCGUtilsExAssignValueFromNoiseTests;
	const UPCGExNoise3DFactoryData* Perlin = MakePerlin();
	if (!TestNotNull(TEXT("A Perlin Noise3D definition can be made"), Perlin)) return false;

	UPCGUtilsExAssignValueFromNoiseSettings* Settings = NewObject<UPCGUtilsExAssignValueFromNoiseSettings>();
	Settings->Seed = 99;
	Settings->Shaping.bRandomOffset = true;
	Settings->Shaping.RandomOffset = 0.15;
	Settings->Shaping.RemapInMin = 0.3;
	Settings->Shaping.RemapInMax = 0.7;
	Settings->MinScalar.Constant = 0.2;
	Settings->MaxScalar.Constant = 0.9;
	Settings->bWriteNoise = true;

	UPCGPointArrayData* Points = MakePoints();
	const TArray<double> Alphas = ExpectedAlphas(Points, Perlin, Settings->Shaping, Settings->Seed);
	FPCGContext Context;
	Run(Settings, Points, Perlin, Context);
	if (!TestEqual(TEXT("One output"), Context.OutputData.TaggedData.Num(), 1)) return false;
	const FPCGTaggedData& Out = Context.OutputData.TaggedData[0];
	const UPCGPointArrayData* OutPoints = Cast<UPCGPointArrayData>(Out.Data);
	if (!TestNotNull(TEXT("Output is point data"), OutPoints)) return false;
	TestTrue(TEXT("Tag kept"), Out.Tags.Contains(TEXT("KeepTag")));
	TestTrue(TEXT("Output is a new object"), OutPoints != Points);

	const auto* Keep = OutPoints->Metadata->GetConstTypedAttribute<int32>(TEXT("Keep"));
	const auto* NoiseAttr = OutPoints->Metadata->GetConstTypedAttribute<double>(TEXT("Noise"));
	if (!TestNotNull(TEXT("Keep attribute kept"), Keep) || !TestNotNull(TEXT("Noise attribute written"), NoiseAttr)) return false;

	const auto Density = OutPoints->GetConstDensityValueRange();
	const auto Keys = OutPoints->GetConstMetadataEntryValueRange();
	bool bSpread = false;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		TestEqual(FString::Printf(TEXT("Density %d = lerp(0.2, 0.9, alpha)"), Index), static_cast<double>(Density[Index]), FMath::Lerp(0.2, 0.9, Alphas[Index]), 1e-5);
		TestEqual(FString::Printf(TEXT("Noise %d"), Index), NoiseAttr->GetValueFromItemKey(Keys[Index]), Alphas[Index], 1e-9);
		TestEqual(FString::Printf(TEXT("Keep %d"), Index), Keep->GetValueFromItemKey(Keys[Index]), 500 + Index);
		bSpread |= !FMath::IsNearlyEqual(Alphas[Index], Alphas[0], 1e-3);
	}
	TestTrue(TEXT("The points land on different noise"), bSpread);
	TestEqual(TEXT("Input untouched"), static_cast<double>(Points->GetConstDensityValueRange()[3]), 1.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGUtilsExAssignValueFromNoiseTypesTest, "PCGUtils.PCGExInterop.AssignValueFromNoise.RotatorAndAttributeOperands", PCGUtilsExAssignValueFromNoiseTests::Flags)
bool FPCGUtilsExAssignValueFromNoiseTypesTest::RunTest(const FString&)
{
	using namespace PCGUtilsExAssignValueFromNoiseTests;
	const UPCGExNoise3DFactoryData* Perlin = MakePerlin();
	if (!TestNotNull(TEXT("Perlin"), Perlin)) return false;

	{
		// Rotator into $Rotation: yaw 0..360 per component, converted to a quaternion.
		UPCGUtilsExAssignValueFromNoiseSettings* Settings = NewObject<UPCGUtilsExAssignValueFromNoiseSettings>();
		Settings->ValueType = EPCGUtilsExNoiseLerpType::Rotator;
		Settings->OutputAttributeName = TEXT("$Rotation");
		UPCGPointArrayData* Points = MakePoints();
		const TArray<double> Alphas = ExpectedAlphas(Points, Perlin, Settings->Shaping, Settings->Seed);
		FPCGContext Context;
		Run(Settings, Points, Perlin, Context);
		if (TestEqual(TEXT("Rotator: one output"), Context.OutputData.TaggedData.Num(), 1))
		{
			const auto* OutPoints = CastChecked<UPCGPointArrayData>(Context.OutputData.TaggedData[0].Data);
			for (int32 Index = 0; Index < 16; ++Index)
			{
				const FQuat Expected = FRotator(0.0, 360.0 * Alphas[Index], 0.0).Quaternion();
				TestTrue(FString::Printf(TEXT("Rotation %d"), Index), OutPoints->GetConstTransformValueRange()[Index].GetRotation().Equals(Expected, 1e-4));
			}
		}
	}
	{
		// Max read from an attribute into a new double attribute.
		UPCGUtilsExAssignValueFromNoiseSettings* Settings = NewObject<UPCGUtilsExAssignValueFromNoiseSettings>();
		Settings->MaxScalar.Input = EPCGExInputValueType::Attribute;
		Settings->MaxScalar.Attribute = TEXT("Tint");
		Settings->OutputAttributeName = TEXT("Result");
		UPCGPointArrayData* Points = MakePoints();
		const TArray<double> Alphas = ExpectedAlphas(Points, Perlin, Settings->Shaping, Settings->Seed);
		FPCGContext Context;
		Run(Settings, Points, Perlin, Context);
		if (TestEqual(TEXT("Attribute Max: one output"), Context.OutputData.TaggedData.Num(), 1))
		{
			const auto* OutPoints = CastChecked<UPCGPointArrayData>(Context.OutputData.TaggedData[0].Data);
			const auto* Result = OutPoints->Metadata->GetConstTypedAttribute<double>(TEXT("Result"));
			if (TestNotNull(TEXT("Result created as double"), Result))
			{
				for (int32 Index = 0; Index < 16; ++Index)
				{
					TestEqual(FString::Printf(TEXT("Result %d"), Index), Result->GetValueFromItemKey(OutPoints->GetConstMetadataEntryValueRange()[Index]), Alphas[Index] * Index, 1e-9);
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGUtilsExAssignValueFromNoiseErrorsTest, "PCGUtils.PCGExInterop.AssignValueFromNoise.Errors", PCGUtilsExAssignValueFromNoiseTests::Flags)
bool FPCGUtilsExAssignValueFromNoiseErrorsTest::RunTest(const FString&)
{
	using namespace PCGUtilsExAssignValueFromNoiseTests;
	const UPCGExNoise3DFactoryData* Perlin = MakePerlin();
	{
		AddExpectedError(TEXT("connect at least one"), EAutomationExpectedErrorFlags::Contains, 1, false);
		FPCGContext Context;
		Run(NewObject<UPCGUtilsExAssignValueFromNoiseSettings>(), MakePoints(), nullptr, Context);
		TestEqual(TEXT("No noise: no output"), Context.OutputData.TaggedData.Num(), 0);
	}
	{
		AddExpectedError(TEXT("Clamp Min (2) is greater than Clamp Max (1)"), EAutomationExpectedErrorFlags::Contains, 1, false);
		UPCGUtilsExAssignValueFromNoiseSettings* Settings = NewObject<UPCGUtilsExAssignValueFromNoiseSettings>();
		Settings->Shaping.ClampMin = 2.0;
		FPCGContext Context;
		Run(Settings, MakePoints(), Perlin, Context);
		TestEqual(TEXT("Bad clamp: no output"), Context.OutputData.TaggedData.Num(), 0);
	}
	{
		AddExpectedError(TEXT("input 0 has no attribute or property 'Falloff' (Scale Attribute Name)"), EAutomationExpectedErrorFlags::Contains, 1, false);
		UPCGUtilsExAssignValueFromNoiseSettings* Settings = NewObject<UPCGUtilsExAssignValueFromNoiseSettings>();
		Settings->Shaping.bScaleByAttribute = true;
		Settings->Shaping.ScaleAttributeName = TEXT("Falloff");
		FPCGContext Context;
		Run(Settings, MakePoints(), Perlin, Context);
		TestEqual(TEXT("Missing scale: no output"), Context.OutputData.TaggedData.Num(), 0);
	}
	{
		AddExpectedError(TEXT("input 0 has no attribute or property 'Nope' (Min)"), EAutomationExpectedErrorFlags::Contains, 1, false);
		UPCGUtilsExAssignValueFromNoiseSettings* Settings = NewObject<UPCGUtilsExAssignValueFromNoiseSettings>();
		Settings->MinScalar.Input = EPCGExInputValueType::Attribute;
		Settings->MinScalar.Attribute = TEXT("Nope");
		FPCGContext Context;
		Run(Settings, MakePoints(), Perlin, Context);
		TestEqual(TEXT("Missing Min attribute: no output"), Context.OutputData.TaggedData.Num(), 0);
	}
	return true;
}

#endif
