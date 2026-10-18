// Copyright Max Harris

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Core/PCGExNoise3DOperation.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Painters/PCGPainterExNoise.h"
#include "UObject/Package.h"

namespace PCGPainterExNoiseTests
{
	/** A Perlin Noise3D definition with its default config. The concrete class is not exported, so make it by name. */
	const UPCGExNoise3DFactoryData* MakePerlin()
	{
		UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/PCGExNoise3D.PCGExNoise3DFactoryPerlin"));
		return Class ? NewObject<UPCGExNoise3DFactoryData>(GetTransientPackage(), Class) : nullptr;
	}

	/** A 5x5 grid of vertices 37 units apart, so neighbouring samples land on clearly different noise. */
	UE::Geometry::FDynamicMesh3 MakeGrid()
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		for (int32 Y = 0; Y < 5; ++Y)
		{
			for (int32 X = 0; X < 5; ++X)
			{
				Mesh.AppendVertex(FVector3d(X * 37.0, Y * 37.0, 11.0));
			}
		}
		for (int32 Y = 0; Y < 4; ++Y)
		{
			for (int32 X = 0; X < 4; ++X)
			{
				const int32 A = Y * 5 + X, B = A + 1, C = A + 6, D = A + 5;
				Mesh.AppendTriangle(A, B, C);
				Mesh.AppendTriangle(A, C, D);
			}
		}
		return Mesh;
	}

	TSharedPtr<FPCGUtilsDynMeshPainterOperation> Prepare(
		const UPCGUtilsDynMeshPainterFactoryData* Factory, const FPCGUtilsDynMeshPainterEvaluationContext& Context)
	{
		TSharedPtr<FPCGUtilsDynMeshPainterOperation> Operation = Factory->CreateOperation(nullptr);
		if (!Operation || !Operation->Initialize(Context) || !Operation->Prepare(Context))
		{
			return nullptr;
		}
		return Operation;
	}

	FPCGUtilsDynMeshPainterValue EvaluateAt(const FPCGUtilsDynMeshPainterOperation& Operation, int32 VertexID)
	{
		FPCGUtilsDynMeshPainterSample Sample;
		Sample.VertexID = VertexID;
		return Operation.Evaluate(Sample);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGPainterExNoiseTest,
	"PCGUtils.PCGExInterop.PainterExNoise.SamplesTheNoiseAtEveryVertex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGPainterExNoiseTest::RunTest(const FString&)
{
	using namespace PCGPainterExNoiseTests;

	const UPCGExNoise3DFactoryData* Perlin = MakePerlin();
	if (!TestNotNull(TEXT("A Perlin Noise3D definition can be made"), Perlin))
	{
		return false;
	}
	const TSharedPtr<FPCGExNoise3DOperation> Reference = Perlin->CreateOperation(nullptr);
	if (!TestTrue(TEXT("The Perlin definition makes an operation"), Reference.IsValid()))
	{
		return false;
	}

	const UE::Geometry::FDynamicMesh3 Mesh = MakeGrid();
	const FTransform Placement(FVector(1000.0, -250.0, 40.0));

	UPCGPainterExNoiseFactoryData* Factory = NewObject<UPCGPainterExNoiseFactoryData>();
	Factory->Noises.Add(Perlin);

	// World space: one noise in the stack paints exactly that noise, sampled at each vertex's world position.
	{
		Factory->Space = EPCGUtilsDynMeshPainterCoordinateSpace::World;
		const FPCGUtilsDynMeshPainterEvaluationContext Context(nullptr, Mesh, Placement);
		const TSharedPtr<FPCGUtilsDynMeshPainterOperation> Op = Prepare(Factory, Context);
		if (!TestNotNull(TEXT("World-space noise prepared"), Op.Get()))
		{
			return false;
		}
		TSet<float> Distinct;
		for (const int32 VertexID : Mesh.VertexIndicesItr())
		{
			const float Painted = EvaluateAt(*Op, VertexID).Scalar;
			const double Expected = Reference->GetDouble(Placement.TransformPosition(FVector(Mesh.GetVertex(VertexID))));
			TestTrue(*FString::Printf(TEXT("Vertex %d paints the Perlin value at its world position"), VertexID),
				FMath::IsNearlyEqual(static_cast<double>(Painted), Expected, 1e-5));
			Distinct.Add(Painted);
		}
		TestTrue(TEXT("The noise varies across the mesh"), Distinct.Num() > 10);
	}

	// Local space ignores the placement.
	{
		Factory->Space = EPCGUtilsDynMeshPainterCoordinateSpace::Local;
		const FPCGUtilsDynMeshPainterEvaluationContext Context(nullptr, Mesh, Placement);
		const TSharedPtr<FPCGUtilsDynMeshPainterOperation> Op = Prepare(Factory, Context);
		if (TestNotNull(TEXT("Local-space noise prepared"), Op.Get()))
		{
			const double Expected = Reference->GetDouble(FVector(Mesh.GetVertex(7)));
			TestTrue(TEXT("Local space samples the mesh's own coordinates"),
				FMath::IsNearlyEqual(static_cast<double>(EvaluateAt(*Op, 7).Scalar), Expected, 1e-5));
		}
	}

	// Color paints the Vector4 noise on only the chosen channels.
	{
		Factory->Space = EPCGUtilsDynMeshPainterCoordinateSpace::Local;
		Factory->ValueType = EPCGUtilsDynMeshPainterValueType::Color;
		Factory->ColorChannels.bBlue = false;
		Factory->ColorChannels.bAlpha = false;
		const FPCGUtilsDynMeshPainterEvaluationContext Context(nullptr, Mesh, FTransform::Identity);
		const TSharedPtr<FPCGUtilsDynMeshPainterOperation> Op = Prepare(Factory, Context);
		if (TestNotNull(TEXT("Color noise prepared"), Op.Get()))
		{
			const FPCGUtilsDynMeshPainterValue Value = EvaluateAt(*Op, 12);
			const FVector4 Expected = Reference->GetVector4(FVector(Mesh.GetVertex(12)));
			TestEqual(TEXT("Color noise defines only R and G"), static_cast<uint8>(Value.ColorChannels),
				static_cast<uint8>(EPCGUtilsDynMeshPainterColorChannel::Red | EPCGUtilsDynMeshPainterColorChannel::Green));
			TestTrue(TEXT("R is the noise's X"), FMath::IsNearlyEqual(static_cast<double>(Value.Color.X), Expected.X, 1e-5));
			TestTrue(TEXT("G is the noise's Y"), FMath::IsNearlyEqual(static_cast<double>(Value.Color.Y), Expected.Y, 1e-5));
		}
	}

	// An empty stack is an error, not a silent zero.
	{
		UPCGPainterExNoiseFactoryData* Empty = NewObject<UPCGPainterExNoiseFactoryData>();
		const FPCGUtilsDynMeshPainterEvaluationContext Context(nullptr, Mesh, FTransform::Identity);
		AddExpectedError(TEXT("none of the 0 connected Noise3D definitions"), EAutomationExpectedErrorFlags::Contains, 1);
		TestNull(TEXT("An empty noise stack fails"), Prepare(Empty, Context).Get());
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
