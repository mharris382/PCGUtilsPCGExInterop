// Copyright Max Harris

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "DynamicMesh/DynamicMesh3.h"
#include "Filters/PCGUtilsExMeshOcclusionFilter.h"
#include "Generators/GridBoxMeshGenerator.h"
#include "Geometry/PCGUtilsDynMeshOcclusion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGUtilsExMeshOcclusionFilterTest, "PCGUtils.PCGExInterop.Filter.MeshOcclusion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPCGUtilsExMeshOcclusionFilterTest::RunTest(const FString&)
{
	using namespace UE::Geometry;
	// A roof over x -50..50, y -50..50, z 100..110.
	FGridBoxMeshGenerator Generator;
	Generator.Box = FOrientedBox3d(FVector3d(0, 0, 105), FVector3d(50, 50, 5));
	Generator.EdgeVertices = FIndex3i(2, 2, 2);
	const FDynamicMesh3 Roof(&Generator.Generate());

	UPCGUtilsExMeshOcclusionFilterFactory* Factory = NewObject<UPCGUtilsExMeshOcclusionFilterFactory>();
	Factory->Occluders = MakeShared<PCGUtilsDynMeshOcclusion::FOccluderSet>();
	Factory->Occluders->Add(Roof, FTransform::Identity);
	const auto Filter = StaticCastSharedPtr<PCGUtilsExMeshOcclusionFilter::FFilter>(Factory->CreateFilter());
	if (!TestTrue(TEXT("Filter created"), Filter.IsValid()))
	{
		return false;
	}

	const FTransform Under(FVector(0, 0, 0));
	const FTransform Beside(FVector(200, 0, 0));
	const FVector Up = FVector::UpVector;

	TestTrue(TEXT("A point under the roof is occluded"), Filter->TestAt(Under, Up, 0.0));
	TestFalse(TEXT("A point beside the roof is not"), Filter->TestAt(Beside, Up, 0.0));
	TestFalse(TEXT("A ray shorter than the gap does not reach"), Filter->TestAt(Under, Up, 50.0));
	TestTrue(TEXT("A negative Max Distance is read as its magnitude"), Filter->TestAt(Under, Up, -150.0));
	TestFalse(TEXT("A zero direction fails the point"), Filter->TestAt(Under, FVector::ZeroVector, 0.0));

	Factory->Config.Direction.bFlip = true;
	TestFalse(TEXT("Flip traces downward, away from the roof"), Filter->TestAt(Under, Up, 0.0));
	Factory->Config.Direction.bFlip = false;

	// A point turned upside down: in point space its up is world down.
	const FTransform Flipped(FRotator(180, 0, 0), FVector(0, 0, 0));
	TestTrue(TEXT("World space ignores the point's rotation"), Filter->TestAt(Flipped, Up, 0.0));
	Factory->Config.bDirectionInPointSpace = true;
	TestFalse(TEXT("Point space follows the point's rotation"), Filter->TestAt(Flipped, Up, 0.0));
	Factory->Config.bDirectionInPointSpace = false;

	Factory->Config.Result = EPCGDynMeshSelfOcclusionResult::Unoccluded;
	TestFalse(TEXT("Unoccluded rejects the covered point"), Filter->TestAt(Under, Up, 0.0));
	TestTrue(TEXT("Unoccluded passes the clear point"), Filter->TestAt(Beside, Up, 0.0));

	TestTrue(TEXT("Constant settings allow proxy evaluation"), Factory->SupportsProxyEvaluation());
	Factory->Config.MaxDistance.Input = EPCGExInputValueType::Attribute;
	TestFalse(TEXT("An attribute setting does not"), Factory->SupportsProxyEvaluation());
	return true;
}

#endif
