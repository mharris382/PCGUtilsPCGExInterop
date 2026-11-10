// Copyright Max Harris

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Core/PCGExContext.h"
#include "Core/PCGExMTCommon.h"
#include "Core/PCGExNoise3DCommon.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Core/PCGExPointFilter.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGPointArrayData.h"
#include "Filters/PCGUtilsExShapedNoiseFilter.h"
#include "Noise/PCGUtilsExNoiseShaping.h"
#include "UObject/Package.h"

namespace PCGUtilsExShapedNoiseFilterTests
{
	struct FReleaseContext
	{
		void operator()(FPCGContext* InContext) const { FPCGContext::Release(InContext); }
	};

	const UPCGExNoise3DFactoryData* MakePerlin()
	{
		UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/PCGExNoise3D.PCGExNoise3DFactoryPerlin"));
		return Class ? NewObject<UPCGExNoise3DFactoryData>(GetTransientPackage(), Class) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGUtilsExShapedNoiseFilterTest, "PCGUtils.PCGExInterop.Filter.ShapedNoise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPCGUtilsExShapedNoiseFilterTest::RunTest(const FString&)
{
	using namespace PCGUtilsExShapedNoiseFilterTests;
	const UPCGExNoise3DFactoryData* Perlin = MakePerlin();
	if (!TestNotNull(TEXT("Perlin"), Perlin)) return false;

	TUniquePtr<FPCGExContext, FReleaseContext> Context(new FPCGExContext());
	FPCGTaggedData& NoiseIn = Context->InputData.TaggedData.Emplace_GetRef();
	NoiseIn.Data = Perlin;
	NoiseIn.Pin = PCGExNoise3D::Labels::SourceNoise3DLabel;

	// 64 points on a grid, so the threshold splits them.
	UPCGPointArrayData* Points = NewObject<UPCGPointArrayData>();
	Points->SetNumPoints(64);
	auto Transforms = Points->GetTransformValueRange();
	auto Seeds = Points->GetSeedValueRange();
	for (int32 Index = 0; Index < 64; ++Index)
	{
		Transforms[Index] = FTransform(FVector((Index % 8) * 41.0, (Index / 8) * 41.0, 3.0));
		Seeds[Index] = 7 * Index + 3;
	}

	UPCGUtilsExShapedNoiseFilterFactory* Factory = NewObject<UPCGUtilsExShapedNoiseFilterFactory>();
	Factory->Config.Shaping.bRandomOffset = true;
	Factory->Config.Shaping.RandomOffset = 0.1;
	Factory->Config.Shaping.RemapInMin = 0.35;
	Factory->Config.Shaping.RemapInMax = 0.65;
	Factory->Config.Comparison.Constant = 0.5; // >= 0.5 by default
	if (!TestTrue(TEXT("Factory initializes with the Noises pin"), Factory->Init(Context.Get()))) return false;

	const TSharedRef<PCGExData::FPointIO> Source = MakeShared<PCGExData::FPointIO>(Context->GetWeakSelfHandle(), Points);
	const TSharedRef<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(Source);
	Facade->bSupportsScopedGet = false;
	const TSharedRef<PCGExPointFilter::FManager> Manager = MakeShared<PCGExPointFilter::FManager>(Facade);
	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> Filters = {Factory};
	if (!TestTrue(TEXT("Manager initializes"), Manager->Init(Context.Get(), Filters))) return false;

	TArray<int8> Results;
	Results.Init(0, 64);
	Manager->Test(PCGExMT::FScope(0, 64), Results, false);

	PCGUtilsExNoise::FFactoryNoiseGenerator Generator;
	TArray<TObjectPtr<const UPCGExNoise3DFactoryData>> Noises = {Perlin};
	Generator.InitFromFactories(Noises);
	int32 NumPassed = 0;
	for (int32 Index = 0; Index < 64; ++Index)
	{
		const double Shaped = Factory->Config.Shaping.Apply(Generator.GetDouble(Transforms[Index].GetLocation()),
			PCGUtilsExNoise::RandomUnit(Seeds[Index], Factory->Config.Seed), 1.0);
		TestEqual(FString::Printf(TEXT("Point %d passes iff shaped noise >= 0.5"), Index), Results[Index] != 0, Shaped >= 0.5);
		NumPassed += Results[Index] != 0;
	}
	TestTrue(TEXT("The threshold splits the points"), NumPassed > 0 && NumPassed < 64);
	return true;
}

#endif
